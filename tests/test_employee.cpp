// University staff ("employee" role): everyone at the university who is not a
// student or teacher, from the VC's office to the bus drivers.
#include "testing.h"
#include "auth/auth.h"
#include "auth/user_store.h"
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

static std::string writeCsv(const char* name, const std::string& rows) {
    fs::path dir = fs::temp_directory_path() / "cafeteria_tests";
    fs::create_directories(dir);
    std::string path = (dir / name).string();
    std::ofstream o(path);
    o << "id,email,role,name,department\n" << rows;
    return path;
}

TEST(employee_role_round_trips) {
    CHECK_EQ(std::string(roleName(Role::Employee)), std::string("employee"));
    Role r = Role::Student;
    CHECK(roleFromName("employee", r));
    CHECK(r == Role::Employee);
}

TEST(uni_employee_blank_email_loads) {
    UniList list;
    uniInit(list);
    std::string err;
    CHECK(uniLoadCsv(list, writeCsv("emp_blank.csv", "3000000000000001,,employee,Rahim Driver,Transport\n"), err));
    UniEntry* e = uniFindById(list, "3000000000000001");
    CHECK(e != nullptr && e->email.empty() && e->role == Role::Employee && e->name == "Rahim Driver");
    uniClear(list);
}

TEST(uni_blank_email_on_student_refused) {
    UniList list;
    uniInit(list);
    std::string err;
    CHECK(!uniLoadCsv(list, writeCsv("emp_blank_student.csv", "1072520005109999,,student,X,CSE\n"), err));
    CHECK(err.find("line 2") != std::string::npos);
    uniClear(list);
}

TEST(uni_duplicate_email_refused) {
    UniList list;
    uniInit(list);
    std::string err;
    CHECK(!uniLoadCsv(list,
                      writeCsv("emp_dup_email.csv", "1072520005101030,a@uni.example,student,A,CSE\n"
                                                    "3000000000000002,A@uni.example,employee,B,Security\n"),
                      err));
    CHECK(err.find("line 3") != std::string::npos);
    CHECK(err.find("duplicate email") != std::string::npos);
    uniClear(list);
}

TEST(users_file_with_employee_round_trips) {
    fs::path dir = fs::temp_directory_path() / "cafeteria_tests" / "emp_users";
    fs::remove_all(dir);
    fs::create_directories(dir);
    UserList users;
    ulInit(users);
    User* u = ulAppend(users);
    u->login = "guard1@uni.example";
    u->universityId = "3000000000000002";
    u->role = Role::Employee;
    u->name = "Sample Guard";
    u->department = "Security";
    u->passwordHash = "$argon2id$v=19$m=65536,t=2,p=1$x$y";
    u->verified = true;
    std::string err;
    CHECK(saveUsers(users, dir.string(), err));
    UserList loaded;
    ulInit(loaded);
    CHECK(loadUsers(loaded, dir.string(), err));
    User* back = ulFind(loaded, "guard1@uni.example");
    CHECK(back != nullptr && back->role == Role::Employee && back->department == "Security");
    ulClear(users);
    ulClear(loaded);
}

// ---- sign-up rules and admin-created accounts --------------------------------

#include "cafe_fixture.h"

struct EmployeeFixture {
    FakeClock clock;
    AuthState auth;
    NotificationList out;
    EmployeeFixture() {
        clock.set(2026, 10, 24, 9, 0, 0);
        AuthConfig cfg = authDefaults();
        cfg.emailDomain = "uni.example";
        authInit(auth, &clock, cfg);
        std::string err;
        CHECK(uniLoadCsv(auth.uni,
                         writeCsv("emp_fixture.csv", "1072520005101030,ayon@uni.example,student,Ayon Das,CSE\n"
                                                     "3000000000000002,guard1@uni.example,employee,Sample Guard,Security\n"
                                                     "3000000000000001,,employee,Rahim Driver,Transport\n"
                                                     "3000000000000003,,employee,Karim Cleaner,Estate\n"),
                         err));
        nlInit(out);
    }
    ~EmployeeFixture() {
        nlClear(out);
        authFree(auth);
    }
    std::string lastCode() {
        std::string body = out.tail ? out.tail->body : "";
        size_t p = body.find("code is ");
        return p == std::string::npos ? "" : body.substr(p + 8, 6);
    }
};

TEST(employee_self_signup_with_university_email) {
    EmployeeFixture f;
    CHECK_EQ(registerStart(f.auth, "guard1@uni.example", "3000000000000002", "guardpass1", f.out), AuthResult::Ok);
    CHECK_EQ(f.out.count, 1);
    CHECK_EQ(registerVerify(f.auth, "guard1@uni.example", f.lastCode(), "guardpass1"), AuthResult::Ok);
    std::string tok;
    CHECK_EQ(login(f.auth, "guard1@uni.example", "guardpass1", false, tok), AuthResult::Ok);
    User* u = ulFind(f.auth.users, "guard1@uni.example");
    CHECK(u != nullptr && u->role == Role::Employee);
}

TEST(blank_email_row_cannot_self_register) {
    EmployeeFixture f;
    CHECK_EQ(registerStart(f.auth, "anyone@uni.example", "3000000000000001", "driverpass1", f.out),
             AuthResult::NoUniversityEmail);
    CHECK_EQ(f.out.count, 0);
    CHECK_EQ(std::string(authMessage(AuthResult::NoUniversityEmail)),
             std::string("You have no university email. Ask the cafeteria office to create your account."));
}

TEST(add_person_creates_active_account) {
    EmployeeFixture f;
    CHECK_EQ(addPerson(f.auth, "3000000000000001", "rahim@example.com", "driverpass1"), AuthResult::Ok);
    std::string tok;
    CHECK_EQ(login(f.auth, "rahim@example.com", "driverpass1", false, tok), AuthResult::Ok);
    User* u = ulFind(f.auth.users, "rahim@example.com");
    CHECK(u != nullptr && u->role == Role::Employee && u->name == "Rahim Driver" && u->department == "Transport" &&
          u->universityId == "3000000000000001" && u->verified);
    CHECK(u->passwordHash.find("driverpass1") == std::string::npos);
}

TEST(add_person_refusals) {
    EmployeeFixture f;
    CHECK_EQ(addPerson(f.auth, "1072520005101030", "ayon@example.com", "goodpass11"), AuthResult::NotEligible);
    CHECK_EQ(addPerson(f.auth, "3000000000000002", "guard@example.com", "goodpass11"), AuthResult::NotEligible);
    CHECK_EQ(addPerson(f.auth, "3999999999999999", "x@example.com", "goodpass11"), AuthResult::IdNotListed);
    CHECK_EQ(addPerson(f.auth, "3000000000000001", "guard1@uni.example", "goodpass11"),
             AuthResult::AlreadyRegistered);  // another row's email
    CHECK_EQ(addPerson(f.auth, "3000000000000001", "short@example.com", "short"), AuthResult::WeakPassword);
    CHECK_EQ(addPerson(f.auth, "3000000000000001", "not an email", "goodpass11"), AuthResult::InvalidInput);
    CHECK_EQ(addPerson(f.auth, "3000000000000001", "rahim@example.com", "goodpass11"), AuthResult::Ok);
    CHECK_EQ(addPerson(f.auth, "3000000000000003", "rahim@example.com", "goodpass11"),
             AuthResult::AlreadyRegistered);  // email used by an account
    CHECK_EQ(addPerson(f.auth, "3000000000000001", "rahim2@example.com", "goodpass11"),
             AuthResult::AlreadyRegistered);  // same ID twice
    CHECK_EQ(f.auth.users.count, 1);
}

TEST(add_person_email_lowercased) {
    EmployeeFixture f;
    CHECK_EQ(addPerson(f.auth, "3000000000000001", "Rahim.Driver@Example.com", "driverpass1"), AuthResult::Ok);
    CHECK(ulFind(f.auth.users, "rahim.driver@example.com") != nullptr);
    std::string tok;
    CHECK_EQ(login(f.auth, "RAHIM.DRIVER@example.com", "driverpass1", false, tok), AuthResult::Ok);
}

TEST(register_after_add_person_refused) {
    EmployeeFixture f;
    CHECK_EQ(addPerson(f.auth, "3000000000000001", "rahim@example.com", "driverpass1"), AuthResult::Ok);
    std::string hash = ulFind(f.auth.users, "rahim@example.com")->passwordHash;
    CHECK_EQ(registerStart(f.auth, "rahim@uni.example", "3000000000000001", "otherpass1", f.out),
             AuthResult::NoUniversityEmail);
    CHECK_EQ(registerStart(f.auth, "rahim@example.com", "3000000000000001", "otherpass1", f.out),
             AuthResult::NotUniversityEmail);
    CHECK_EQ(f.out.count, 0);
    CHECK(ulFind(f.auth.users, "rahim@example.com")->passwordHash == hash);
}

TEST(employee_same_limit_as_teacher) {
    CafeFixture f;  // per-person limit 2
    Reservation* r = nullptr;
    CHECK_EQ(reserve(f.caf, "guard1@uni.example", TEHARI, 2, f.out, &r), Result::Ok);
    CHECK_EQ(reserve(f.caf, "guard1@uni.example", TEHARI, 1, f.out, &r), Result::UserLimit);
    CHECK_EQ(reserve(f.caf, "teacher1@uni.example", TEHARI, 2, f.out, &r), Result::Ok);
    CHECK_EQ(reserve(f.caf, "teacher1@uni.example", TEHARI, 1, f.out, &r), Result::UserLimit);
}
