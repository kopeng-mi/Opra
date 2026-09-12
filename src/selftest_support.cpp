// The counters behind selftest.h. Deliberately dependency-free: a module's test file links this and
// nothing else, so orbital math can be checked with no SDL, no device and no window.
#include "selftest.h"

#include <cmath>
#include <cstdio>

namespace opra::selftest {
namespace {

int g_failures = 0;
int g_checks = 0;

}  // namespace

void check(bool ok, const char *what) {
    ++g_checks;
    if (ok) return;
    ++g_failures;
    std::printf("  FAIL  %s\n", what);
}

void check_close(double actual, double expected, double tolerance, const char *what) {
    const double delta = std::fabs(actual - expected);
    ++g_checks;
    if (delta <= tolerance) return;
    ++g_failures;
    std::printf("  FAIL  %s: got %.17g, expected %.17g (delta %.3g)\n", what, actual, expected,
                delta);
}

int failures() { return g_failures; }
int checks() { return g_checks; }

}  // namespace opra::selftest
