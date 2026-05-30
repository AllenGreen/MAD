#pragma once

#include <cstdio>
#include <functional>
#include <string>
#include <vector>

namespace mad::test {

struct Test {
    std::string name;
    std::function<bool()> fn;
};

inline std::vector<Test>& registry() {
    static std::vector<Test> tests;
    return tests;
}

struct Registrar {
    Registrar(const char* name, std::function<bool()> fn) {
        registry().push_back({name, std::move(fn)});
    }
};

inline int run_all() {
    int passed = 0, failed = 0;
    for (auto& [name, fn] : registry()) {
        std::fprintf(stderr, "[ RUN  ] %s\n", name.c_str());
        if (fn()) {
            std::fprintf(stderr, "[ PASS ] %s\n", name.c_str());
            passed++;
        } else {
            std::fprintf(stderr, "[ FAIL ] %s\n", name.c_str());
            failed++;
        }
    }
    std::fprintf(stderr, "\n%d passed, %d failed\n", passed, failed);
    return failed > 0 ? 1 : 0;
}

} // namespace mad::test

#define TEST(name) \
    static bool test_##name(); \
    static mad::test::Registrar reg_##name(#name, test_##name); \
    static bool test_##name()

#define ASSERT_TRUE(expr) do { \
    if (!(expr)) { \
        std::fprintf(stderr, "  FAIL: %s:%d: %s\n", __FILE__, __LINE__, #expr); \
        return false; \
    } \
} while(0)

#define ASSERT_EQ(a, b) do { \
    if ((a) != (b)) { \
        std::fprintf(stderr, "  FAIL: %s:%d: %s != %s\n", __FILE__, __LINE__, #a, #b); \
        return false; \
    } \
} while(0)
