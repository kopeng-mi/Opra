// Module tests for the system layer: sim/system_tests.cpp. Wired into run_selftest by selftest.cpp.
#pragma once

namespace opra {

/** System file loading, Kepler propagation, SOI selection and belt generation. Returns failures. */
int system_tests();

}  // namespace opra
