#pragma once

// Tiny header-only test harness — no external dependency.
// Usage:
//   #include "test_runner.hpp"
//   bool TestXxx() { LD_EXPECT(cond); LD_EXPECT_EQ(a, b); return true; }
//   int main() { LD_RUN(TestXxx); return loradriver::test::report(); }

#include <cstdio>
#include <cstdlib>

namespace loradriver::test {

inline int& fail_count() { static int n = 0; return n; }
inline int& pass_count() { static int n = 0; return n; }

inline int report() {
    std::fprintf(stderr, "[summary] passed=%d failed=%d\n",
                 pass_count(), fail_count());
    return fail_count() == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}

}  // namespace loradriver::test

#define LD_EXPECT(cond)                                                       \
    do {                                                                      \
        if (!(cond)) {                                                        \
            std::fprintf(stderr, "  EXPECT failed: %s  (%s:%d)\n",            \
                         #cond, __FILE__, __LINE__);                          \
            return false;                                                     \
        }                                                                     \
    } while (0)

#define LD_EXPECT_EQ(a, b)                                                    \
    do {                                                                      \
        const auto _la = (a);                                                 \
        const auto _lb = (b);                                                 \
        if (!(_la == _lb)) {                                                  \
            std::fprintf(stderr, "  EXPECT_EQ failed: %s != %s  (%s:%d)\n",   \
                         #a, #b, __FILE__, __LINE__);                         \
            return false;                                                     \
        }                                                                     \
    } while (0)

#define LD_RUN(fn)                                                            \
    do {                                                                      \
        std::fprintf(stderr, "[run] %s\n", #fn);                              \
        if ((fn)()) {                                                         \
            ++loradriver::test::pass_count();                                 \
        } else {                                                              \
            ++loradriver::test::fail_count();                                 \
            std::fprintf(stderr, "[FAIL] %s\n", #fn);                         \
        }                                                                     \
    } while (0)
