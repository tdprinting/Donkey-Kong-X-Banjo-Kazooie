// Minimal test runner shared by the test executables.
#pragma once

#include <cstdio>
#include <functional>
#include <utility>
#include <vector>

namespace {

int g_failures = 0;
std::vector<std::pair<const char*, std::function<void()>>>& registry() {
    static std::vector<std::pair<const char*, std::function<void()>>> tests;
    return tests;
}

struct Register {
    Register(const char* name, std::function<void()> fn) { registry().emplace_back(name, std::move(fn)); }
};

#define TEST(name)                                     \
    static void name();                                \
    static Register register_##name(#name, name);      \
    static void name()

#define CHECK(cond)                                                              \
    do {                                                                         \
        if (!(cond)) {                                                           \
            std::fprintf(stderr, "  %s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #cond); \
            g_failures++;                                                        \
        }                                                                        \
    } while (0)

} // namespace

#define JIGGY_TEST_MAIN                                                           \
    int main() {                                                                  \
        for (auto& [name, fn] : registry()) {                                     \
            const int before = g_failures;                                        \
            fn();                                                                 \
            std::printf("%s %s\n", g_failures == before ? "PASS" : "FAIL", name); \
        }                                                                         \
        if (g_failures) {                                                         \
            std::printf("%d check(s) failed\n", g_failures);                     \
            return 1;                                                             \
        }                                                                         \
        std::printf("all %zu tests passed\n", registry().size());                \
        return 0;                                                                 \
    }
