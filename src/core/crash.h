// Crash reporting: the shipping build must not die silently. A breadcrumb names the phase the
// frame was in, and the unhandled-exception filter writes the code, the faulting address and the
// breadcrumb to a small file next to the settings.
#pragma once

namespace opra {

/** Names the phase the frame is about to enter. Cheap: one pointer store. */
void set_phase(const char *phase);

/** Installs the handler. Called once, before the loop starts. */
void install_crash_handler();

/** The last phase, for the debug log. */
const char *last_phase();

}  // namespace opra
