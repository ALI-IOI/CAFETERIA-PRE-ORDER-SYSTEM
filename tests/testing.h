// Minimal test harness. Tests register themselves into a hand-built linked
// list (no STL containers), and test_main.cpp runs them in file order.
#pragma once
#include <string>
#include <type_traits>

struct TestCase {
    const char* name;
    void (*fn)();
    TestCase* next;
};

int registerTest(TestCase* t);
void reportFailure(const char* file, int line, const char* expr, const std::string& detail);

namespace testing {
inline std::string show(const std::string& s) { return "\"" + s + "\""; }
inline std::string show(const char* s) { return s ? show(std::string(s)) : "(null)"; }
inline std::string show(bool b) { return b ? "true" : "false"; }
inline std::string show(const void* p) { return p ? "(ptr)" : "(null)"; }
template <class T>
typename std::enable_if<std::is_arithmetic<T>::value || std::is_enum<T>::value, std::string>::type
show(T v) { return std::to_string((long long)v); }
}  // namespace testing

#define TEST(name)                                                  \
    static void name();                                             \
    static TestCase name##_case = {#name, name, nullptr};           \
    static int name##_reg = registerTest(&name##_case);             \
    static void name()

#define CHECK(cond)                                                 \
    do {                                                            \
        if (!(cond)) reportFailure(__FILE__, __LINE__, #cond, "");  \
    } while (0)

#define CHECK_EQ(a, b)                                                          \
    do {                                                                        \
        auto _va = (a);                                                         \
        auto _vb = (b);                                                         \
        if (!(_va == _vb))                                                      \
            reportFailure(__FILE__, __LINE__, #a " == " #b,                     \
                          testing::show(_va) + " vs " + testing::show(_vb));    \
    } while (0)
