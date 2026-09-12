// The assertion vocabulary every module's test file shares, plus the runner main.cpp calls.
#pragma once

namespace opra {

/** Runs every suite: selftest.cpp's own cases then each module's. Returns the failure count. */
int run_selftest();

namespace selftest {

/** Records one assertion. `what` names the invariant, not the call that checked it. */
void check(bool ok, const char *what);

/** Records one assertion with a tolerance; a failure prints both numbers and the delta. */
void check_close(double actual, double expected, double tolerance, const char *what);

int failures();
int checks();

}  // namespace selftest

}  // namespace opra
