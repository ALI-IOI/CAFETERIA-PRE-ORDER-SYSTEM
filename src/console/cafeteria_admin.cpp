// Administrator tool: staff accounts are created here, never through the web
// (proposal section 5, decision D10). Run it on the server machine while the
// server is stopped.
//
//   cafeteria_admin [--data DIR] add-staff <login> "<full name>"
//   cafeteria_admin [--data DIR] [--config DIR] add-person <university ID> <email>
//
// add-person creates the account of university staff who have no university
// email (bus drivers, guards, ...): the ID must be an employee row with a blank
// email in <config>/university_list.csv; the email can be a personal one.
//
// The password is asked twice and is not echoed when typed at a console.
#include "auth/auth.h"
#include "auth/user_store.h"
#include "auth/validation.h"
#include "storage/file_util.h"
#include "storage/lock_file.h"
#include <cstdio>
#include <iostream>
#include <string>
#ifdef _WIN32
#include <windows.h>
#endif

static std::string readPassword(const char* prompt) {
    std::fprintf(stderr, "%s", prompt);
    std::string pw;
#ifdef _WIN32
    HANDLE h = GetStdHandle(STD_INPUT_HANDLE);
    DWORD mode = 0;
    bool console = GetConsoleMode(h, &mode) != 0;
    if (console) SetConsoleMode(h, mode & ~ENABLE_ECHO_INPUT);
    std::getline(std::cin, pw);
    if (console) {
        SetConsoleMode(h, mode);
        std::fprintf(stderr, "\n");
    }
#else
    std::getline(std::cin, pw);
#endif
    if (!pw.empty() && pw.back() == '\r') pw.pop_back();
    return pw;
}

static int usage() {
    std::fprintf(stderr,
                 "Usage: cafeteria_admin [--data DIR] add-staff <login> \"<full name>\"\n"
                 "       cafeteria_admin [--data DIR] [--config DIR] add-person <university ID> <email>\n");
    return 2;
}

int main(int argc, char** argv) {
    std::string dataDir = "data", configDir = "config";
    int i = 1;
    while (i + 1 < argc && (std::string(argv[i]) == "--data" || std::string(argv[i]) == "--config")) {
        (std::string(argv[i]) == "--data" ? dataDir : configDir) = argv[i + 1];
        i += 2;
    }
    if (argc - i != 3) return usage();
    std::string command = argv[i];
    if (command != "add-staff" && command != "add-person") return usage();
    std::string arg1 = argv[i + 1], arg2 = argv[i + 2];
    bool person = command == "add-person";

    if (lockStatus(dataDir + "/server.lock") == LockState::Live) {
        std::fprintf(stderr, "The server is running (%s/server.lock exists). Stop it first.\n", dataDir.c_str());
        return 1;
    }

    SystemClock clock;
    AuthState auth;
    authInit(auth, &clock, authDefaults());
    std::string err;
    if (!loadUsers(auth.users, dataDir, err)) {
        std::fprintf(stderr, "Cannot load users: %s\n", err.c_str());
        authFree(auth);
        return 1;
    }
    if (person && !uniLoadCsv(auth.uni, configDir + "/university_list.csv", err)) {
        std::fprintf(stderr, "University list error: %s\n", err.c_str());
        authFree(auth);
        return 1;
    }

    std::string pw1 = readPassword(person ? "Starting password: " : "Password for new staff account: ");
    std::string pw2 = readPassword("Repeat password: ");
    int rc = 0;
    if (pw1 != pw2) {
        std::fprintf(stderr, "Passwords do not match.\n");
        rc = 1;
    } else {
        AuthResult r = person ? addPerson(auth, arg1, arg2, pw1) : addStaff(auth, arg1, arg2, pw1);
        if (r != AuthResult::Ok) {
            const char* why = authMessage(r);
            if (r == AuthResult::InvalidInput)
                why = person ? "that is not a valid email address"
                             : "login must be 3-32 of a-z 0-9 . _ - and name 1-60 characters";
            std::fprintf(stderr, "Refused: %s\n", why);
            rc = 1;
        } else if (!saveUsers(auth.users, dataDir, err)) {
            std::fprintf(stderr, "Cannot save users: %s\n", err.c_str());
            rc = 1;
        } else if (person) {
            std::printf("University staff account '%s' created. They can change the password on My account.\n",
                        toLower(arg2).c_str());
        } else {
            std::printf("Staff account '%s' created.\n", arg1.c_str());
        }
    }
    authFree(auth);
    return rc;
}
