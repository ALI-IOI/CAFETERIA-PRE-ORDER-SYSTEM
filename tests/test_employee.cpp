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
