#include "email/email.h"
#include "auth/validation.h"
#include "storage/file_util.h"
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <fstream>
#ifdef _WIN32
#include <windows.h>
#endif

bool loadSmtpConfig(const std::string& path, SmtpConfig& cfg, std::string& err) {
    std::ifstream in(path);
    if (!in) return true;
    std::string line, f[2];
    int lineNo = 0;
    while (std::getline(in, line)) {
        lineNo++;
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty() || line[0] == '#') continue;
        size_t eq = line.find('=');
        if (eq == std::string::npos) {
            err = path + " line " + std::to_string(lineNo) + ": expected key=value";
            return false;
        }
        std::string key = line.substr(0, eq), value = line.substr(eq + 1);
        if (key == "host") cfg.host = value;
        else if (key == "port") {
            if (!parseIntStrict(value, cfg.port) || cfg.port < 1 || cfg.port > 65535) {
                err = path + " line " + std::to_string(lineNo) + ": bad port";
                return false;
            }
        } else if (key == "sender") cfg.sender = value;
        else if (key == "username") cfg.username = value;
        else if (key == "curl") cfg.curlPath = value;
        else {
            err = path + " line " + std::to_string(lineNo) + ": unknown key " + key;
            return false;
        }
    }
    return true;
}

bool smtpConfigured(const SmtpConfig& cfg) {
    return !cfg.host.empty() && cfg.host.find("[CONFIRM") == std::string::npos && validEmail(cfg.sender);
}

// ---- outbox -------------------------------------------------------------------

OutboxSender::OutboxSender(const std::string& dir) : dir_(dir), seq_(0) {
    std::string err;
    ensureDir(dir_, err);
    std::ifstream idx(dir_ + "/index.log");
    std::string line;
    while (std::getline(idx, line)) {
        if (!line.empty()) seq_++;
    }
}

bool OutboxSender::send(const Notification& n) {
    std::lock_guard<std::mutex> lock(mu_);
    seq_++;
    char num[16];
    std::snprintf(num, sizeof num, "%06d", seq_);
    std::string name = std::string(num) + "_" + notifTypeName(n.type) + ".txt";
    std::string err;
    std::string content = "To: " + n.to + "\nSubject: " + n.subject + "\n\n" + n.body + "\n";
    if (!atomicWrite(dir_ + "/" + name, content, err)) return false;
    std::string subjectOneLine = n.subject;
    for (char& c : subjectOneLine) {
        if (c == '\n' || c == '\r' || c == '|') c = ' ';
    }
    return appendFile(dir_ + "/index.log",
                      std::string(num) + "|" + notifTypeName(n.type) + "|" + n.to + "|" +
                          std::to_string(n.reservationId) + "|" + subjectOneLine + "\n",
                      err);
}

// ---- SMTP via curl.exe ------------------------------------------------------------

static bool hasCRLF(const std::string& s) {
    return s.find('\r') != std::string::npos || s.find('\n') != std::string::npos;
}

bool buildSmtpMessage(const SmtpConfig& cfg, const Notification& n, std::string& out) {
    if (!validEmail(n.to) || hasCRLF(n.subject) || hasCRLF(cfg.sender)) return false;
    char date[64];
    std::time_t t = std::time(nullptr);
    std::tm g = {};
#ifdef _WIN32
    gmtime_s(&g, &t);
#else
    gmtime_r(&t, &g);
#endif
    std::strftime(date, sizeof date, "%a, %d %b %Y %H:%M:%S +0000", &g);

    out = "From: BAUSTK Cafeteria <" + cfg.sender + ">\r\n" + "To: " + n.to + "\r\n" + "Subject: " + n.subject +
          "\r\n" + "Date: " + date + "\r\n" + "MIME-Version: 1.0\r\n" +
          "Content-Type: text/plain; charset=UTF-8\r\n\r\n";
    for (char c : n.body) {
        if (c == '\n') out += "\r\n";
        else if (c != '\r') out += c;
    }
    out += "\r\n";
    return true;
}

static std::string quoteArg(const std::string& s) {
    return "\"" + s + "\"";  // inputs are validated: no quotes, spaces in paths only
}

std::string buildCurlCommandLine(const SmtpConfig& cfg, const std::string& to, const std::string& messagePath,
                                 const std::string& curlConfigPath) {
    return quoteArg(cfg.curlPath) + " --silent --show-error --ssl-reqd --max-time 60" + " --url " +
           quoteArg("smtps://" + cfg.host + ":" + std::to_string(cfg.port)) + " --mail-from " +
           quoteArg(cfg.sender) + " --mail-rcpt " + quoteArg(to) + " --upload-file " + quoteArg(messagePath) +
           " -K " + quoteArg(curlConfigPath);
}

SmtpCurlSender::SmtpCurlSender(const SmtpConfig& cfg, const std::string& tmpDir)
    : cfg_(cfg), tmpDir_(tmpDir), seq_(0) {
    std::string err;
    ensureDir(tmpDir_, err);
}

// curl config syntax: backslash and double quote must be escaped inside "...".
static std::string curlConfigEscape(const std::string& s) {
    std::string out;
    for (char c : s) {
        if (c == '\\' || c == '"') out += '\\';
        out += c;
    }
    return out;
}

bool SmtpCurlSender::send(const Notification& n) {
    std::string msg;
    if (!buildSmtpMessage(cfg_, n, msg)) return false;
    const char* pw = std::getenv("CAFE_SMTP_PASSWORD");
    std::string user = cfg_.username.empty() ? cfg_.sender : cfg_.username;

    int seq;
    {
        std::lock_guard<std::mutex> lock(mu_);
        seq = ++seq_;
    }
    std::string base = tmpDir_ + "/mail" + std::to_string(seq);
    std::string msgPath = base + ".eml", cfgPath = base + ".cfg", err;
    if (!atomicWrite(msgPath, msg, err)) return false;
    if (!atomicWrite(cfgPath, "user = \"" + curlConfigEscape(user + ":" + (pw ? pw : "")) + "\"\n", err)) {
        std::remove(msgPath.c_str());
        return false;
    }
    bool ok = false;
#ifdef _WIN32
    std::string cmd = buildCurlCommandLine(cfg_, n.to, msgPath, cfgPath);
    STARTUPINFOA si = {};
    si.cb = sizeof si;
    PROCESS_INFORMATION pi = {};
    // No shell is involved: CreateProcess runs curl.exe directly.
    if (CreateProcessA(nullptr, &cmd[0], nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {
        if (WaitForSingleObject(pi.hProcess, 90 * 1000) == WAIT_OBJECT_0) {
            DWORD code = 1;
            GetExitCodeProcess(pi.hProcess, &code);
            ok = code == 0;
        } else {
            TerminateProcess(pi.hProcess, 1);
        }
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }
#endif
    std::remove(cfgPath.c_str());
    std::remove(msgPath.c_str());
    return ok;
}

EmailSender* makeSender(const SmtpConfig& cfg, const std::string& outboxDir) {
    if (smtpConfigured(cfg)) return new SmtpCurlSender(cfg, outboxDir + "/smtp-tmp");
    return new OutboxSender(outboxDir);
}
