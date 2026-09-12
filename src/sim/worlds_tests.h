// Assertions for the worlds work: the heightfield, the air, the touchdown gate, the hoverslam and
// the survey (plan 03 sections 3.2-3.7). src/sim/worlds_tests.cpp.
#pragma once

namespace opra {

/** Terrain, atmosphere, descent, survey. Returns the failure count. */
int worlds_tests();

}  // namespace opra
