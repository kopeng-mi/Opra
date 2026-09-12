// Module tests for the modular ship data model: src/sim/component_tests.cpp.
#pragma once

namespace opra {

/** The three stock component sets against SHIPS, the drive cone's edge, the centre of mass, and a
 *  set with no drives. Returns failures. */
int component_tests();

}  // namespace opra
