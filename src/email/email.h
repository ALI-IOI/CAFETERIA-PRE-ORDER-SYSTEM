// Email delivery (proposal section 6: "SMTP through libcurl").
//
// OutboxSender writes each email to a file; it is used until a real SMTP
// account is configured ([CONFIRM] in config/smtp.cfg) and by the tests.
// SmtpCurlSender runs Windows' curl.exe (which is libcurl) without a shell.
#pragma once
#include "core/notification.h"
#include <mutex>
#include <string>

struct SmtpConfig {
    std::string host;      // [CONFIRM]
    int port = 465;        // 465 = SMTPS
    std::string sender;    // [CONFIRM] From address
    std::string username;  // defaults to sender
    std::string curlPath = "curl.exe";
    // The password is never stored in a file: it is read from the
    // CAFE_SMTP_PASSWORD environment variable.
};

// key=value file; '#' comments. Missing file -> empty config (outbox mode).
bool loadSmtpConfig(const std::string& path, SmtpConfig& cfg, std::string& err);
bool smtpConfigured(const SmtpConfig& cfg);  // false if empty or still "[CONFIRM..."

struct EmailSender {
    virtual bool send(const Notification& n) = 0;
    virtual const char* modeName() const = 0;
    virtual ~EmailSender() {}
};

struct OutboxSender : EmailSender {
    explicit OutboxSender(const std::string& dir);
    bool send(const Notification& n) override;
    const char* modeName() const override { return "outbox"; }

  private:
    std::string dir_;
    int seq_;
    std::mutex mu_;  // send() is called from several request threads
};

struct SmtpCurlSender : EmailSender {
    SmtpCurlSender(const SmtpConfig& cfg, const std::string& tmpDir);
    bool send(const Notification& n) override;
    const char* modeName() const override { return "smtp"; }

  private:
    SmtpConfig cfg_;
    std::string tmpDir_;
    int seq_;
    std::mutex mu_;
};

// Outbox when SMTP is not configured; caller deletes the result.
EmailSender* makeSender(const SmtpConfig& cfg, const std::string& outboxDir);

// Pure helpers (tested directly).
// Fails if the recipient is not a valid email or the subject has CR/LF.
bool buildSmtpMessage(const SmtpConfig& cfg, const Notification& n, std::string& out);
std::string buildCurlCommandLine(const SmtpConfig& cfg, const std::string& to, const std::string& messagePath,
                                 const std::string& curlConfigPath);
