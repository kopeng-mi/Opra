// Module tests for the collider and RCS work: sim/shapes_tests.cpp. Wired into run_selftest by
// selftest.cpp.
#pragma once

namespace opra {

/** Compound collider, single-MTV resolution, and the per-jet RCS solution. Returns failures. */
int shapes_tests();

}  // namespace opra
