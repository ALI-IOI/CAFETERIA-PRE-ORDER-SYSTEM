#include "testing.h"
#include "auth/validation.h"
#include "storage/file_util.h"

TEST(valid_email_accepts_normal_rejects_bad) {
    CHECK(validEmail("alice@uni.example"));
    CHECK(validEmail("a.b-c_d+1@cse.uni.example"));
    CHECK(!validEmail(""));
    CHECK(!validEmail("alice"));
    CHECK(!validEmail("alice@"));
    CHECK(!validEmail("@uni.example"));
    CHECK(!validEmail("a@@uni.example"));
    CHECK(!validEmail("a b@uni.example"));
    CHECK(!validEmail("alice@uni..example"));
    CHECK(!validEmail("alice@uni.example\r\nBcc: x@y.z"));
    CHECK(!validEmail("alice|x@uni.example"));
    CHECK(!validEmail(std::string(120, 'a') + "@uni.example"));
}

TEST(email_domain_is_lowercased) {
    CHECK_EQ(emailDomain("Alice@UNI.Example"), std::string("uni.example"));
    CHECK_EQ(toLower("AbC"), std::string("abc"));
}

TEST(university_id_digits_only) {
    CHECK(validUniversityId("9990000000000102"));
    CHECK(!validUniversityId("12345"));          // too short
    CHECK(!validUniversityId("12345678901234567890123"));
    CHECK(!validUniversityId("10725A0005"));
    CHECK(!validUniversityId("-9990000000"));
    CHECK(!validUniversityId(""));
}

TEST(long_text_over_limit) {
    CHECK(validName("Ayon Das"));
    CHECK(!validName(""));
    CHECK(!validName(std::string(61, 'x')));
    CHECK(validPassword("correct horse"));
    CHECK(!validPassword("short"));
    CHECK(!validPassword(std::string(129, 'p')));
}

TEST(pipe_and_newline_rejected) {
    CHECK(!validName("Ayon|Das"));
    CHECK(!validName("Ayon\nDas"));
    CHECK(!validName("Ayon\tDas"));
    CHECK(!validPassword("password\n123"));
    CHECK(!validStaffLogin("staff|1"));
}

TEST(special_characters_in_names_allowed_but_safe) {
    CHECK(validName("O'Brien <script>"));  // stored as text; pages insert it with textContent
    CHECK(validStaffLogin("cafe.staff-1"));
    CHECK(!validStaffLogin("Cafe Staff"));
    CHECK(!validStaffLogin("ab"));
}

TEST(negative_and_non_numeric_quantity) {
    int v = 0;
    CHECK(parseIntStrict("2", v));
    CHECK_EQ(v, 2);
    CHECK(parseIntStrict("-3", v));
    CHECK_EQ(v, -3);
    CHECK(!parseIntStrict("", v));
    CHECK(!parseIntStrict("+1", v));
    CHECK(!parseIntStrict("1e3", v));
    CHECK(!parseIntStrict("12abc", v));
    CHECK(!parseIntStrict(" 5", v));
    CHECK(!parseIntStrict("-", v));
    CHECK(!parseIntStrict("99999999999", v));  // int overflow
}
