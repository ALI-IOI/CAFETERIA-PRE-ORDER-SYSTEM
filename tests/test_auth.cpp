#include "testing.h"
#include "auth/auth.h"
#include "auth/crypto.h"
#include "auth/user_store.h"
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

static std::string writeUniList(const char* name) {
    fs::path dir = fs::temp_directory_path() / "cafeteria_tests";
    fs::create_directories(dir);
    std::string path = (dir / name).string();
    std::ofstream o(path);
    o << "# fixture university list\n"
      << "id,email,role,name,department\n"
      << "1072520005101030,ayon@uni.example,student,Ayon Das,CSE\n"
      << "1072520005101050,ali@uni.example,student,Mohammad Ali Gazi,CSE\n"
      << "2000000000000001,khalid@uni.example,teacher,Md. Mahamudul Hasan Khalid,CSE\n";
    return path;
}

struct AuthFixture {
    FakeClock clock;
    AuthState auth;
    NotificationList out;
    AuthFixture() {
        clock.set(2026, 10, 23, 9, 0, 0);
        AuthConfig cfg = authDefaults();
        cfg.emailDomain = "uni.example";
        authInit(auth, &clock, cfg);
        std::string err;
        bool ok = uniLoadCsv(auth.uni, writeUniList("uni_list.csv"), err);
        CHECK(ok);
        nlInit(out);
    }
    ~AuthFixture() {
        nlClear(out);
        authFree(auth);
    }
    // The 6-digit code from the last verification email.
    std::string lastCode() {
        std::string body = out.tail ? out.tail->body : "";
        size_t p = body.find("code is ");
        return p == std::string::npos ? "" : body.substr(p + 8, 6);
    }
    void registerAndVerify(const char* email, const char* id, const char* pw) {
        CHECK_EQ(registerStart(auth, email, id, pw, out), AuthResult::Ok);
        CHECK_EQ(registerVerify(auth, email, lastCode(), pw), AuthResult::Ok);
    }
};

TEST(non_university_email_rejected) {
    AuthFixture f;
    CHECK_EQ(registerStart(f.auth, "ayon@gmail.com", "1072520005101030", "longpassword", f.out),
             AuthResult::NotUniversityEmail);
    CHECK_EQ(f.out.count, 0);
}

TEST(registration_disabled_until_domain_configured) {
    FakeClock clock;
    AuthState a;
    authInit(a, &clock, authDefaults());  // domain not set
    NotificationList out;
    nlInit(out);
    CHECK_EQ(registerStart(a, "ayon@uni.example", "1072520005101030", "longpassword", out),
             AuthResult::NotUniversityEmail);
    authFree(a);
}

TEST(unlisted_id_rejected) {
    AuthFixture f;
    CHECK_EQ(registerStart(f.auth, "ayon@uni.example", "9999999999999999", "longpassword", f.out),
             AuthResult::IdNotListed);
}

TEST(id_email_mismatch_rejected) {
    AuthFixture f;
    CHECK_EQ(registerStart(f.auth, "ali@uni.example", "1072520005101030", "longpassword", f.out),
             AuthResult::EmailIdMismatch);
}

TEST(role_comes_from_list) {
    AuthFixture f;
    f.registerAndVerify("Khalid@UNI.example", "2000000000000001", "teacherpass1");
    User* u = ulFind(f.auth.users, "khalid@uni.example");
    CHECK(u != nullptr);
    CHECK_EQ(u->role, Role::Teacher);
    CHECK_EQ(u->department, std::string("CSE"));
    f.registerAndVerify("ayon@uni.example", "1072520005101030", "studentpass1");
    CHECK_EQ(ulFind(f.auth.users, "ayon@uni.example")->role, Role::Student);
}

TEST(verification_code_correct_wrong_expired) {
    AuthFixture f;
    CHECK_EQ(registerStart(f.auth, "ayon@uni.example", "1072520005101030", "longpassword", f.out),
             AuthResult::Ok);
    CHECK_EQ(f.out.tail->type, NotifType::VerificationCode);
    CHECK_EQ(f.out.tail->to, std::string("ayon@uni.example"));
    std::string code = f.lastCode();
    CHECK_EQ(code.size(), (size_t)6);
    std::string wrong = code == "000000" ? "111111" : "000000";
    CHECK_EQ(registerVerify(f.auth, "ayon@uni.example", wrong, "longpassword"), AuthResult::BadCode);
    CHECK_EQ(registerVerify(f.auth, "ayon@uni.example", code, "longpassword"), AuthResult::Ok);
    CHECK(ulFind(f.auth.users, "ayon@uni.example")->verified);
    CHECK_EQ(registerVerify(f.auth, "ayon@uni.example", code, "longpassword"), AuthResult::AlreadyRegistered);

    // expired code
    registerStart(f.auth, "ali@uni.example", "1072520005101050", "longpassword", f.out);
    std::string code2 = f.lastCode();
    f.clock.advanceMinutes(16);
    CHECK_EQ(registerVerify(f.auth, "ali@uni.example", code2, "longpassword"), AuthResult::CodeExpired);
}

TEST(verification_code_locked_after_5_wrong_attempts) {
    AuthFixture f;
    registerStart(f.auth, "ayon@uni.example", "1072520005101030", "longpassword", f.out);
    std::string code = f.lastCode();
    std::string wrong = code == "000000" ? "111111" : "000000";
    for (int i = 0; i < 5; i++) registerVerify(f.auth, "ayon@uni.example", wrong, "longpassword");
    CHECK_EQ(registerVerify(f.auth, "ayon@uni.example", code, "longpassword"), AuthResult::CodeExpired);
}

TEST(login_before_verify_refused) {
    AuthFixture f;
    registerStart(f.auth, "ayon@uni.example", "1072520005101030", "longpassword", f.out);
    std::string token;
    CHECK_EQ(login(f.auth, "ayon@uni.example", "longpassword", false, token), AuthResult::NotVerified);
    CHECK(token.empty());
}

TEST(hash_is_salted) {
    CHECK(cryptoInit());
    std::string h1 = hashPassword("same password");
    std::string h2 = hashPassword("same password");
    CHECK(h1 != h2);
    CHECK(h1.find("$argon2id$") == 0);
    CHECK(verifyPassword(h1, "same password"));
    CHECK(verifyPassword(h2, "same password"));
    CHECK(!verifyPassword(h1, "other password"));
}

TEST(login_success_creates_session) {
    AuthFixture f;
    f.registerAndVerify("ayon@uni.example", "1072520005101030", "studentpass1");
    std::string token;
    CHECK_EQ(login(f.auth, "AYON@uni.example", "studentpass1", false, token), AuthResult::Ok);
    Session* s = sessionGet(f.auth, token);
    CHECK(s != nullptr);
    CHECK_EQ(s->login, std::string("ayon@uni.example"));
    CHECK_EQ(s->role, Role::Student);
}

TEST(lockout_after_5_failures_then_unlock_after_15min) {
    AuthFixture f;
    f.registerAndVerify("ayon@uni.example", "1072520005101030", "studentpass1");
    std::string token;
    for (int i = 0; i < 4; i++)
        CHECK_EQ(login(f.auth, "ayon@uni.example", "wrongpass", false, token), AuthResult::BadCredentials);
    CHECK_EQ(login(f.auth, "ayon@uni.example", "wrongpass", false, token), AuthResult::Locked);
    // even the right password is refused while locked
    CHECK_EQ(login(f.auth, "ayon@uni.example", "studentpass1", false, token), AuthResult::Locked);
    f.clock.advanceMinutes(15);
    CHECK_EQ(login(f.auth, "ayon@uni.example", "studentpass1", false, token), AuthResult::Ok);
}

TEST(unknown_user_is_bad_credentials) {
    AuthFixture f;
    std::string token;
    CHECK_EQ(login(f.auth, "nobody@uni.example", "whatever1", false, token), AuthResult::BadCredentials);
}

TEST(staff_cannot_use_public_login_and_vice_versa) {
    AuthFixture f;
    CHECK_EQ(addStaff(f.auth, "cafe.staff", "Cafeteria Staff", "staffpass123"), AuthResult::Ok);
    CHECK_EQ(addStaff(f.auth, "cafe.staff", "Again", "staffpass123"), AuthResult::AlreadyRegistered);
    f.registerAndVerify("ayon@uni.example", "1072520005101030", "studentpass1");
    std::string token;
    CHECK_EQ(login(f.auth, "cafe.staff", "staffpass123", false, token), AuthResult::BadCredentials);
    CHECK_EQ(login(f.auth, "ayon@uni.example", "studentpass1", true, token), AuthResult::BadCredentials);
    CHECK_EQ(login(f.auth, "cafe.staff", "staffpass123", true, token), AuthResult::Ok);
    CHECK_EQ(sessionGet(f.auth, token)->role, Role::Staff);
}

TEST(staff_login_cannot_be_registered_publicly) {
    AuthFixture f;
    // there is no public path that creates Role::Staff: registerStart only uses list roles
    CHECK_EQ(registerStart(f.auth, "cafe.staff", "1072520005101030", "longpassword", f.out),
             AuthResult::NotUniversityEmail);
}

TEST(session_expires) {
    AuthFixture f;
    f.registerAndVerify("ayon@uni.example", "1072520005101030", "studentpass1");
    std::string token;
    login(f.auth, "ayon@uni.example", "studentpass1", false, token);
    f.clock.advanceMinutes(8 * 60);
    CHECK(sessionGet(f.auth, token) == nullptr);
    CHECK_EQ(f.auth.sessions.count, 0);
}

TEST(logout_removes_session) {
    AuthFixture f;
    f.registerAndVerify("ayon@uni.example", "1072520005101030", "studentpass1");
    std::string token;
    login(f.auth, "ayon@uni.example", "studentpass1", false, token);
    logout(f.auth, token);
    CHECK(sessionGet(f.auth, token) == nullptr);
    CHECK(sessionGet(f.auth, "") == nullptr);
    CHECK(sessionGet(f.auth, "not-a-token") == nullptr);
}

TEST(tokens_unique_and_32_bytes) {
    CHECK(cryptoInit());
    std::string a = randomToken(32), b = randomToken(32);
    CHECK_EQ(a.size(), (size_t)64);  // hex
    CHECK(a != b);
}

TEST(users_round_trip_through_file) {
    AuthFixture f;
    f.registerAndVerify("ayon@uni.example", "1072520005101030", "studentpass1");
    addStaff(f.auth, "cafe.staff", "Cafeteria Staff", "staffpass123");
    fs::path dir = fs::temp_directory_path() / "cafeteria_tests" / "users_rt";
    fs::remove_all(dir);
    std::string err;
    CHECK(saveUsers(f.auth.users, dir.string(), err));

    UserList loaded;
    ulInit(loaded);
    CHECK(loadUsers(loaded, dir.string(), err));
    CHECK_EQ(loaded.count, 2);
    User* u = ulFind(loaded, "ayon@uni.example");
    CHECK(u != nullptr);
    CHECK(u->verified);
    CHECK_EQ(u->role, Role::Student);
    CHECK_EQ(u->name, std::string("Ayon Das"));
    CHECK(verifyPassword(u->passwordHash, "studentpass1"));
    CHECK_EQ(ulFind(loaded, "cafe.staff")->role, Role::Staff);
    ulClear(loaded);
}

TEST(bad_university_list_line_reported) {
    fs::path p = fs::temp_directory_path() / "cafeteria_tests" / "bad_uni.csv";
    {
        std::ofstream o(p);
        o << "id,email,role,name,department\n1072520005101030,ayon@uni.example,admin,Ayon,CSE\n";
    }
    UniList list;
    uniInit(list);
    std::string err;
    CHECK(!uniLoadCsv(list, p.string(), err));
    CHECK(err.find("line 2") != std::string::npos);
    uniClear(list);
}

// Review finding 2: someone who knows a student's email and ID re-registers
// during the pending window with their own password. The student then enters
// the newest code from their inbox. That must NOT verify the attacker's password.
TEST(pending_registration_cannot_be_taken_over) {
    AuthFixture f;
    CHECK_EQ(registerStart(f.auth, "ayon@uni.example", "1072520005101030", "victim-password", f.out), AuthResult::Ok);
    CHECK_EQ(registerStart(f.auth, "ayon@uni.example", "1072520005101030", "attacker-password", f.out), AuthResult::Ok);
    std::string newestCode = f.lastCode();  // arrives in the victim's inbox
    CHECK_EQ(registerVerify(f.auth, "ayon@uni.example", newestCode, "victim-password"), AuthResult::BadCode);
    CHECK(!ulFind(f.auth.users, "ayon@uni.example")->verified);
    std::string token;
    CHECK(login(f.auth, "ayon@uni.example", "attacker-password", false, token) != AuthResult::Ok);
    // the victim simply registers again and completes it
    CHECK_EQ(registerStart(f.auth, "ayon@uni.example", "1072520005101030", "victim-password", f.out), AuthResult::Ok);
    CHECK_EQ(registerVerify(f.auth, "ayon@uni.example", f.lastCode(), "victim-password"), AuthResult::Ok);
    CHECK_EQ(login(f.auth, "ayon@uni.example", "victim-password", false, token), AuthResult::Ok);
}
