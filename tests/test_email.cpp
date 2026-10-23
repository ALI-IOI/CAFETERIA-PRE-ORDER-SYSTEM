#include "testing.h"
#include "email/email.h"
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

static Notification note(NotifType t, const char* to, const char* subject, const char* body) {
    Notification n;
    n.type = t;
    n.to = to;
    n.subject = subject;
    n.body = body;
    n.reservationId = 7;
    n.next = nullptr;
    return n;
}

static std::string slurp(const fs::path& p) {
    std::ifstream in(p);
    std::string s, line;
    while (std::getline(in, line)) s += line + "\n";
    return s;
}

TEST(outbox_writes_one_file_per_notification_with_to_subject_body) {
    fs::path dir = fs::temp_directory_path() / "cafeteria_tests" / "outbox";
    fs::remove_all(dir);
    OutboxSender sender(dir.string());
    CHECK(sender.send(note(NotifType::Ready, "alice@uni.example", "Your Tehari (R-0007) is ready",
                           "Your Tehari (R-0007) is ready for collection.")));
    CHECK(sender.send(note(NotifType::Reminder, "bob@uni.example", "Reminder", "About 6 hours left.")));
    int files = 0;
    std::string firstContent;
    for (auto& e : fs::directory_iterator(dir)) {
        if (e.path().extension() == ".txt") {
            files++;
            if (e.path().filename().string().find("000001_ready") == 0) firstContent = slurp(e.path());
        }
    }
    CHECK_EQ(files, 2);
    CHECK(firstContent.find("To: alice@uni.example") != std::string::npos);
    CHECK(firstContent.find("Subject: Your Tehari (R-0007) is ready") != std::string::npos);
    CHECK(firstContent.find("ready for collection") != std::string::npos);
    std::string index = slurp(dir / "index.log");
    CHECK(index.find("000001|ready|alice@uni.example|7|") != std::string::npos);
    CHECK(index.find("000002|reminder|bob@uni.example|7|") != std::string::npos);
}

TEST(outbox_numbering_continues_after_restart) {
    fs::path dir = fs::temp_directory_path() / "cafeteria_tests" / "outbox_restart";
    fs::remove_all(dir);
    {
        OutboxSender s(dir.string());
        s.send(note(NotifType::Ready, "a@uni.example", "s", "b"));
    }
    OutboxSender s2(dir.string());
    s2.send(note(NotifType::Ready, "a@uni.example", "s", "b"));
    CHECK(slurp(dir / "index.log").find("000002|ready|") != std::string::npos);
}

TEST(smtp_message_has_headers_and_crlf) {
    SmtpConfig cfg;
    cfg.host = "smtp.example.org";
    cfg.port = 465;
    cfg.sender = "cafeteria@uni.example";
    std::string msg;
    CHECK(buildSmtpMessage(cfg, note(NotifType::Ready, "alice@uni.example", "Ready", "Line 1\nLine 2"), msg));
    CHECK(msg.find("From: BAUSTK Cafeteria <cafeteria@uni.example>\r\n") != std::string::npos);
    CHECK(msg.find("To: alice@uni.example\r\n") != std::string::npos);
    CHECK(msg.find("Subject: Ready\r\n") != std::string::npos);
    CHECK(msg.find("\r\n\r\nLine 1\r\nLine 2") != std::string::npos);
}

TEST(header_injection_newline_in_subject_rejected) {
    SmtpConfig cfg;
    cfg.host = "smtp.example.org";
    cfg.port = 465;
    cfg.sender = "cafeteria@uni.example";
    std::string msg;
    CHECK(!buildSmtpMessage(cfg, note(NotifType::Ready, "alice@uni.example", "Hi\r\nBcc: evil@x.y", "b"), msg));
    CHECK(!buildSmtpMessage(cfg, note(NotifType::Ready, "alice@uni.example\r\nBcc: e@x.y", "Hi", "b"), msg));
    CHECK(!buildSmtpMessage(cfg, note(NotifType::Ready, "not an email", "Hi", "b"), msg));
}

TEST(smtp_command_line_has_no_shell_metacharacters_from_input) {
    SmtpConfig cfg;
    cfg.host = "smtp.example.org";
    cfg.port = 465;
    cfg.sender = "cafeteria@uni.example";
    std::string cmd = buildCurlCommandLine(cfg, "alice@uni.example", "C:\\tmp\\m.eml", "C:\\tmp\\m.cfg");
    CHECK(cmd.find("\"smtps://smtp.example.org:465\"") != std::string::npos);
    CHECK(cmd.find("--mail-rcpt \"alice@uni.example\"") != std::string::npos);
    CHECK(cmd.find("--ssl-reqd") != std::string::npos);
    CHECK(cmd.find("-K \"C:\\tmp\\m.cfg\"") != std::string::npos);  // password goes in a config file
    CHECK(cmd.find("&") == std::string::npos);
    CHECK(cmd.find("|") == std::string::npos);
}

TEST(smtp_config_with_placeholder_uses_outbox) {
    fs::path dir = fs::temp_directory_path() / "cafeteria_tests" / "smtpcfg";
    fs::create_directories(dir);
    {
        std::ofstream o(dir / "smtp.cfg");
        o << "# [CONFIRM] fill in\nhost=[CONFIRM: SMTP host]\nport=465\nsender=[CONFIRM: sender]\n";
    }
    SmtpConfig cfg;
    std::string err;
    CHECK(loadSmtpConfig((dir / "smtp.cfg").string(), cfg, err));
    CHECK(!smtpConfigured(cfg));
    EmailSender* s = makeSender(cfg, (dir / "outbox").string());
    CHECK(dynamic_cast<OutboxSender*>(s) != nullptr);
    delete s;
}

// The outbox folder may be missing (or blocked) when the sender is created and
// appear later: numbering must continue after the files already there.
TEST(outbox_counts_existing_files_once_folder_is_usable) {
    fs::path dir = fs::temp_directory_path() / "cafeteria_tests" / "outbox_late";
    fs::remove_all(dir);
    fs::create_directories(dir.parent_path());
    { std::ofstream blocker(dir.string()); blocker << "not a folder"; }  // a file where the folder should be
    OutboxSender s(dir.string());
    CHECK(!s.send(note(NotifType::Ready, "a@uni.example", "s", "b")));   // cannot write
    fs::remove(dir);
    fs::create_directories(dir);
    {
        std::ofstream idx((dir / "index.log").string());
        idx << "000001|ready|x@uni.example|1|old\n000002|ready|y@uni.example|2|old\n";
    }
    CHECK(s.send(note(NotifType::Ready, "a@uni.example", "s", "b")));
    CHECK(slurp(dir / "index.log").find("000003|ready|a@uni.example") != std::string::npos);
}
