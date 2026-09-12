#include "game/warp.h"

#include <algorithm>

namespace opra {

double Warp::rate() const {
    const int index = std::max(0, std::min(WARP_RATE_COUNT - 1, step));
    return WARP_RATES[index];
}

bool Warp::railed() const { return rate() > WARP_RAIL_ABOVE; }

void Warp::request(int delta) {
    step = std::max(0, std::min(WARP_RATE_COUNT - 1, step + delta));
    if (step == 0) drop = Drop::None;
}

bool Warp::update(bool thrusting, bool in_contact, bool switched_sphere, bool in_air) {
    // Thrust or a contact above 10x would be smeared across the step: the integrator never sees a
    // warp-sized dt, so the honest thing is to come back to real time and let the pilot burn. Air is
    // the same argument with more force behind it: below the atmosphere's top there is no conic to
    // ride, so the rail has nothing exact to advance.
    if (thrusting || in_contact || switched_sphere || in_air) {
        drop = thrusting      ? Drop::Thrust
               : in_contact   ? Drop::Contact
               : switched_sphere ? Drop::Sphere
                              : Drop::Air;
        if (step != 0) {
            step = 0;
            return true;
        }
    }
    return false;
}

bool Warp::drop_to_real_time(Drop reason) {
    if (step == 0) return false;
    step = 0;
    drop = reason;
    return true;
}

}  // namespace opra
