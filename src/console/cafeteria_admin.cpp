// Administrator tool: staff accounts are created here, never through the web
// (proposal section 5, decision D10). Run it on the server machine while the
// server is stopped.
//
//   cafeteria_admin [--data DIR] add-staff <login> "<full name>"
//
// The password is asked twice and is not echoed when typed at a console.
#include "auth/auth.h"
#include "auth/user_store.h"
#include "storage/file_util.h"
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
    std::fprintf(stderr, "Usage: cafeteria_admin [--data DIR] add-staff <login> \"<full name>\"\n");
    return 2;
}

int main(int argc, char** argv) {
    std::string dataDir = "data";
    int i = 1;
    if (i + 1 < argc && std::string(argv[i]) == "--data") {
        dataDir = argv[i + 1];
        i += 2;
    }
    if (argc - i != 3 || std::string(argv[i]) != "add-staff") return usage();
    std::string login = argv[i + 1], name = argv[i + 2];

    if (fileExists(dataDir + "/server.lock")) {
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

    std::string pw1 = readPassword("Password for new staff account: ");
    std::string pw2 = readPassword("Repeat password: ");
    int rc = 0;
    if (pw1 != pw2) {
        std::fprintf(stderr, "Passwords do not match.\n");
        rc = 1;
    } else {
        AuthResult r = addStaff(auth, login, name, pw1);
        if (r != AuthResult::Ok) {
            std::fprintf(stderr, "Refused: %s\n",
                         r == AuthResult::InvalidInput
                             ? "login must be 3-32 of a-z 0-9 . _ - and name 1-60 characters"
                             : authMessage(r));
            rc = 1;
        } else if (!saveUsers(auth.users, dataDir, err)) {
            std::fprintf(stderr, "Cannot save users: %s\n", err.c_str());
            rc = 1;
        } else {
            std::printf("Staff account '%s' created.\n", login.c_str());
        }
    }
    authFree(auth);
    return rc;
}
