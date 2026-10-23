#include "testing.h"

TEST(harness_passing_check) {
    CHECK(1 + 1 == 2);
    CHECK_EQ(std::string("a"), std::string("a"));
}

