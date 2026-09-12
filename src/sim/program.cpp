#include "sim/program.h"

#include <algorithm>
#include <cmath>

#include "orbit/transfer.h"

namespace opra {
namespace {

constexpr double PI = 3.14159265358979323846;
constexpr double TAU = 2.0 * PI;
constexpr double DEG = PI / 180.0;

/** The plan's touchdown gate (plan 3.4), in the units the panel prints. */
constexpr double TOUCHDOWN_VERTICAL = 4.0;   // m/s
constexpr double TOUCHDOWN_LATERAL = 1.5;    // m/s
constexpr double TOUCHDOWN_TILT = 8.0;       // degrees off local up
constexpr double TOUCHDOWN_DISTANCE = 12.0;  // metres from the pad's centre
constexpr double TOUCHDOWN_RATE = 4.0;       // degrees per second

/**
 * The approach's closing-rate profile, v = sqrt(2 a_max d) damped by the plan's 0.6. The throttle
 * turns a rate into a setting by asking for that rate one horizon ahead: a proportional law, and
 * the reason the commanded value is worth drawing beside the actual one.
 */
constexpr double APPROACH_DAMPING = 0.6;
constexpr double THROTTLE_HORIZON = 1.0;

/** How far the land cue tilts the nose into a lateral velocity, per m/s of it, and its cap. */
constexpr double LATERAL_TILT_PER_SPEED = 0.12;
constexpr double LATERAL_TILT_MAX = 0.45;

/** A conic counts as matched when its radius is this close to the target's, as a fraction. */
constexpr double MATCH_BAND = 0.01;

/** The periapsis after a deorbit burn may miss the datum by this much and still count as sized. */
constexpr double DEORBIT_SLACK = 1000.0;

/** The placement gate's limit: how far the descent point may be from the pad's own angle. */
constexpr double DEORBIT_PLACEMENT = 5.0;

double wrap_pi(double angle) {
    double wrapped = std::fmod(angle + PI, TAU);
    if (wrapped < 0.0) wrapped += TAU;
    return wrapped - PI;
}

double clamp01(double value) { return std::clamp(value, 0.0, 1.0); }

/** The bearing of a world direction, in the frame every collar mark uses. */
Real bearing(const glm::dvec2& direction) { return std::atan2(direction.y, direction.x); }

glm::dvec2 zone_velocity(const World& world) {
    return glm::dvec2(world.ship.velocity.x, world.ship.velocity.y);
}

/** The ship's conic about the deepest body holding it, plus the geometry every row reads. */
struct Conic {
    bool valid = false;
    int index = -1;  // the body's place in SystemDef::bodies
    const Body* body = nullptr;
    double mu = 0.0;
    double radius = 0.0;  // the mean radius: the datum every altitude here is measured from
    glm::dvec2 r{0.0};
    glm::dvec2 v{0.0};
    orbit::Elements elements{};
};

Conic conic_of(const World& world, double t) {
    Conic out;
    if (world.system.bodies.empty() || world.bodies.empty()) return out;
    const int index = world.primary < 0 ? 0 : world.primary;
    if (index >= static_cast<int>(world.system.bodies.size())) return out;
    if (index >= static_cast<int>(world.bodies.size())) return out;
    const Body& body = world.system.bodies[static_cast<size_t>(index)];
    if (body.mu <= 0.0) return out;
    out.index = index;
    out.body = &body;
    out.mu = body.mu;
    out.radius = body.radius;
    out.r = world.system_position() - world.bodies[static_cast<size_t>(index)].position;
    out.v = world.system_velocity() - world.bodies[static_cast<size_t>(index)].velocity;
    out.elements = orbit::from_state(out.r, out.v, out.mu, t);
    out.valid = true;
    return out;
}

/** The hull's maximum acceleration now, at the mass it is carrying now. */
double max_acceleration(const World& world) {
    if (!world.ship.spec) return 0.0;
    return world.ship.spec->thrust / (world.ship.spec->mass + world.ship.fuel);
}

/** The mean anomaly at a true anomaly, through the eccentric anomaly. */
double mean_at_true(double true_anomaly, double e) {
    const double eccentric = 2.0 * std::atan2(std::sqrt(1.0 - e) * std::sin(true_anomaly * 0.5),
                                              std::sqrt(1.0 + e) * std::cos(true_anomaly * 0.5));
    return eccentric - e * std::sin(eccentric);
}

/** Seconds from now to the mean anomaly `m` of this conic, wrapped into one period. */
double time_to_mean_anomaly(const orbit::Elements& elements, double t, double m) {
    const double n = orbit::mean_motion(elements);
    if (!(n > 0.0)) return 0.0;
    const double now = elements.M0 + n * (t - elements.t0);
    double delta = std::fmod(m - now, TAU);
    if (delta < 0.0) delta += TAU;
    return delta / n;
}

/**
 * The pad the deorbit and the descent aim at: the body's own seat when it names one, else the first
 * the system file declared. Null when the body has no pads - which is to say, no landing target.
 */
const Pad* target_pad(const Body& body) {
    if (body.terrain.pads.empty()) return nullptr;
    if (body.surface.present && body.surface.pad >= 0 &&
        body.surface.pad < static_cast<int>(body.terrain.pads.size())) {
        return &body.terrain.pads[static_cast<size_t>(body.surface.pad)];
    }
    return &body.terrain.pads.front();
}

/** The conic after a retrograde impulse of `dv` at the current state. */
orbit::Elements after_retrograde(const Conic& conic, double t, double dv) {
    const double speed = glm::length(conic.v);
    const double left = std::max(0.0, speed - dv);
    const glm::dvec2 v = speed > 0.0 ? conic.v * (left / speed) : conic.v;
    return orbit::from_state(conic.r, v, conic.mu, t);
}

/** Sizes a retrograde impulse so the periapsis sits on the datum, and returns the impulse. */
double deorbit_sizing(const Conic& conic, double t) {
    const double datum = conic.radius;
    if (orbit::periapsis(conic.elements) <= datum) return 0.0;
    // The periapsis drops monotonically as the burn grows, and killing the whole orbital speed
    // drops it to the body's centre, so the bracket exists for any conic starting above the
    // surface.
    double low = 0.0;
    double high = glm::length(conic.v);
    for (int i = 0; i < 64; ++i) {
        const double mid = 0.5 * (low + high);
        if (orbit::periapsis(after_retrograde(conic, t, mid)) > datum) {
            low = mid;
        } else {
            high = mid;
        }
    }
    return 0.5 * (low + high);
}

Cue circularize(const Conic& conic, double t, double a_max) {
    Cue cue;
    if (!conic.valid || !orbit::is_elliptic(conic.elements)) return cue;
    const double e = conic.elements.e;
    const double r_apo = orbit::apoapsis(conic.elements);
    if (!(r_apo > 0.0)) return cue;
    const double v_apo = std::sqrt(conic.mu * (1.0 - e) / (conic.elements.a * (1.0 + e)));
    cue.active = true;
    cue.heading = bearing(conic.v);  // prograde
    cue.dv_remaining = std::max(0.0, std::sqrt(conic.mu / r_apo) - v_apo);
    cue.burn_in = time_to_mean_anomaly(conic.elements, t, PI);
    cue.burn_for = a_max > 0.0 ? cue.dv_remaining / a_max : 0.0;
    cue.throttle = 1.0;
    cue.step = "burn at apoapsis";
    return cue;
}

Cue match_orbit(const World& world, const Conic& conic, double t, double a_max) {
    Cue cue;
    if (!conic.valid) return cue;
    if (world.anchor_body < 0) return cue;
    if (world.anchor_body >= static_cast<int>(world.system.bodies.size())) return cue;
    if (world.anchor_body >= static_cast<int>(world.bodies.size())) return cue;
    const Body& seat = world.system.bodies[static_cast<size_t>(world.anchor_body)];
    const double r_now = glm::length(conic.r);
    const double r_seat = seat.elements.a;
    if (!(r_now > 0.0) || !(r_seat > 0.0)) return cue;
    cue.active = true;
    if (std::fabs(r_now - r_seat) <= r_seat * MATCH_BAND) {
        // Inside the band the departure is behind the ship, so the cue is the arrival burn. That is
        // the whole of the sequencing the director needs: the state it reads is the ship's own
        // orbit, so nothing has to be carried from frame to frame (F9).
        const double dv = std::sqrt(conic.mu / r_now) - glm::length(conic.v);
        cue.heading = dv >= 0.0 ? bearing(conic.v) : bearing(-conic.v);
        cue.dv_remaining = std::fabs(dv);
        cue.step = "circularise at the target radius";
    } else {
        const orbit::Hohmann plan = orbit::hohmann(r_now, r_seat, conic.mu);
        const glm::dvec2 seat_r = world.bodies[static_cast<size_t>(world.anchor_body)].position -
                                  world.bodies[static_cast<size_t>(conic.index)].position;
        const double lead = wrap_pi(std::atan2(seat_r.y, seat_r.x) - std::atan2(conic.r.y, conic.r.x));
        const double n_seat = orbit::mean_motion(seat.elements);
        const double required = orbit::phase_angle_required(n_seat, plan.transfer_time);
        cue.burn_in = orbit::time_to_window(lead, required, orbit::mean_motion(conic.elements), n_seat);
        if (!std::isfinite(cue.burn_in)) {
            // The two periods coincide: the lead never repeats, so there is no window to count to.
            cue.burn_in = 0.0;
            cue.step = "same period - hold";
        } else {
            cue.step = "departure window";
        }
        cue.dv_remaining = plan.dv1;
        cue.heading = bearing(conic.v);
    }
    cue.burn_for = a_max > 0.0 ? cue.dv_remaining / a_max : 0.0;
    cue.throttle = 1.0;
    return cue;
}

Cue approach(const World& world, double a_max) {
    Cue cue;
    const std::vector<Contact> contacts = world.contacts();
    const int at = contact_index(contacts, world.target);
    if (at < 0) return cue;
    const Contact& target = contacts[static_cast<size_t>(at)];
    const glm::dvec2 to(target.position.x - world.ship.position.x,
                        target.position.y - world.ship.position.y);
    const double range = glm::length(to);
    if (!(range > 1.0e-6)) return cue;
    const glm::dvec2 axis = to / range;
    // Every contact in this build is fixed in the sector's frame, so the closing rate is the
    // ship's own velocity along the line of sight.
    const double closing = glm::dot(zone_velocity(world), axis);
    const double desired = std::sqrt(2.0 * a_max * range) * APPROACH_DAMPING;
    cue.active = true;
    cue.heading = bearing(axis);
    cue.throttle = clamp01((desired - closing) / (a_max * THROTTLE_HORIZON));
    cue.dv_remaining = std::fabs(desired - closing);
    cue.burn_for = a_max > 0.0 ? cue.dv_remaining / a_max : 0.0;
    cue.step = closing > desired ? "brake" : (range < 60.0 ? "close" : "transfer");
    return cue;
}

Cue dock(const World& world, double a_max) {
    Cue cue;
    if (world.target_port < 0 || world.station_ports.empty()) return cue;
    if (world.target_port >= static_cast<int>(world.station_ports.size())) return cue;
    const PortState& berth = world.target_state;
    const glm::dvec2 offset(berth.position.x - world.ship_port_state.position.x,
                            berth.position.y - world.ship_port_state.position.y);
    const double range = glm::length(offset);
    const GateLimits limits{};
    const ApproachGate& gate = world.gate;

    // The nose: the hull's own port has to face the berth, which means the berth's outward normal
    // and the port's local normal end up antiparallel. The engine is on the nose, so this is the
    // attitude the pilot holds through the whole corridor.
    const glm::dvec2 want(-berth.normal.x, -berth.normal.y);
    const double angle = std::atan2(want.y, want.x) -
                         std::atan2(world.ship_port.local_normal.y, world.ship_port.local_normal.x);
    const double desired =
        std::min(limits.closing_max * 0.75,
                 std::sqrt(2.0 * a_max * std::max(0.0, range)) * APPROACH_DAMPING);

    cue.active = true;
    cue.heading = bearing(glm::dvec2(-std::sin(angle), std::cos(angle)));
    cue.throttle = clamp01((desired - gate.closing) / (a_max * THROTTLE_HORIZON));
    cue.dv_remaining = std::fabs(desired - gate.closing);
    cue.gates.push_back({"axial", gate.axial, limits.axial_max, "m", gate.axial_ok});
    cue.gates.push_back({"lateral", gate.lateral, limits.lateral_max, "m", gate.lateral_ok});
    cue.gates.push_back({"closing", gate.closing, limits.closing_max, "m/s", gate.closing_ok});
    cue.gates.push_back(
        {"align", gate.alignment / DEG, limits.alignment_max / DEG, "deg", gate.alignment_ok});
    cue.gates.push_back({"rate", gate.rate / DEG, limits.rate_max / DEG, "deg/s", gate.rate_ok});
    // The caret names the one term to work on, in the order the corridor is flown.
    if (!gate.closing_ok) {
        cue.step = "match velocity";
    } else if (!gate.alignment_ok) {
        cue.step = "align corridor";
    } else if (!gate.axial_ok || !gate.lateral_ok) {
        cue.step = "close";
    } else if (!gate.rate_ok) {
        cue.step = "kill rotation";
    } else {
        cue.step = "capture";
    }
    return cue;
}

Cue deorbit(const Conic& conic, double t, double a_max) {
    Cue cue;
    if (!conic.valid || !conic.body->terrain.present) return cue;
    const Pad* pad = target_pad(*conic.body);
    if (!pad) return cue;
    if (!(glm::length(conic.r) > conic.radius)) return cue;

    const double dv = deorbit_sizing(conic, t);
    const orbit::Elements after = after_retrograde(conic, t, dv);
    // A retrograde impulse puts the periapsis half a revolution behind the burn; the residual is
    // the conic's own departure from that, and it carries to any other burn point by symmetry.
    const double offset = wrap_pi(after.omega - (std::atan2(conic.r.y, conic.r.x) + PI));
    const double burn_angle = wrap_pi(pad->theta - PI - offset);
    const double nu_burn = wrap_pi(burn_angle - conic.elements.omega);

    cue.active = true;
    cue.heading = bearing(-conic.v);  // retrograde
    cue.dv_remaining = dv;
    cue.burn_for = a_max > 0.0 ? dv / a_max : 0.0;
    cue.burn_in = orbit::is_elliptic(conic.elements)
                      ? time_to_mean_anomaly(conic.elements, t,
                                             mean_at_true(nu_burn, conic.elements.e))
                      : 0.0;
    cue.throttle = 1.0;
    cue.step = cue.burn_in > 0.5 ? "wait for the burn point" : "burn retrograde";

    const double periapsis = orbit::periapsis(after) - conic.radius;
    const double placement = std::fabs(wrap_pi(after.omega - pad->theta)) / DEG;
    cue.gates.push_back(
        {"periapsis", periapsis, 0.0, "m", std::fabs(periapsis) <= DEORBIT_SLACK});
    cue.gates.push_back(
        {"pad", placement, DEORBIT_PLACEMENT, "deg", placement <= DEORBIT_PLACEMENT});
    return cue;
}

Cue land(const World& world, const Conic& conic, double a_max) {
    Cue cue;
    if (!conic.valid || !conic.body->terrain.present) return cue;
    const Pad* pad = target_pad(*conic.body);
    if (!pad) return cue;
    const double r_now = glm::length(conic.r);
    if (!(r_now > 1.0)) return cue;

    const glm::dvec2 up = conic.r / r_now;
    const double altitude = r_now - conic.radius;
    const glm::dvec2 velocity = zone_velocity(world);
    const double v_down = -glm::dot(velocity, up);
    const glm::dvec2 lateral_v = velocity - up * glm::dot(velocity, up);
    const double lateral = glm::length(lateral_v);
    const double g = conic.mu / (r_now * r_now);
    const double a_net = a_max - g;
    const double descent = std::max(0.0, v_down);
    const double h_burn = a_net > 0.0 ? descent * descent / (2.0 * a_net) : 0.0;
    const double t_burn = a_net > 0.0 ? descent / a_net : 0.0;

    // The nose: local up, tilted into the lateral correction. The engine is on the nose, so the
    // tilt is the only lateral authority there is.
    glm::dvec2 correction{0.0};
    if (lateral > 1.0e-3) {
        correction = (lateral_v / lateral) * -std::min(LATERAL_TILT_MAX, lateral * LATERAL_TILT_PER_SPEED);
    }
    const glm::dvec2 nose = glm::normalize(up + correction);

    cue.active = true;
    cue.heading = bearing(nose);
    cue.dv_remaining = std::hypot(descent, lateral);
    cue.burn_for = t_burn;
    if (altitude > h_burn) {
        // Coasting to the burn: the countdown is the free-fall time from here to `h_burn`.
        cue.throttle = 0.0;
        cue.burn_in = g > 0.0 ? (std::sqrt(std::max(0.0, descent * descent + 2.0 * g * (altitude - h_burn))) -
                                descent) / g
                              : 0.0;
        cue.step = "coast to the burn";
    } else {
        const double required = altitude > 1.0 ? descent * descent / (2.0 * altitude) + g : g;
        cue.throttle = a_max > 0.0 ? clamp01(required / a_max) : 0.0;
        cue.burn_in = 0.0;
        cue.step = "burn to touchdown";
    }

    // The touchdown gate: the plan's table, against the hull as it is actually flying.
    const glm::dvec2 ship_nose(-std::sin(world.ship.angle), std::cos(world.ship.angle));
    const double tilt = std::acos(std::clamp(glm::dot(ship_nose, up), -1.0, 1.0)) / DEG;
    const double arc = std::fabs(wrap_pi(std::atan2(conic.r.y, conic.r.x) - pad->theta)) * conic.radius;
    const double rate = std::fabs(world.ship.angularVelocity) / DEG;
    cue.gates.push_back({"vertical", descent, TOUCHDOWN_VERTICAL, "m/s", v_down < TOUCHDOWN_VERTICAL});
    cue.gates.push_back({"lateral", lateral, TOUCHDOWN_LATERAL, "m/s", lateral < TOUCHDOWN_LATERAL});
    cue.gates.push_back({"tilt", tilt, TOUCHDOWN_TILT, "deg", tilt < TOUCHDOWN_TILT});
    cue.gates.push_back({"pad", arc, TOUCHDOWN_DISTANCE, "m", arc < TOUCHDOWN_DISTANCE});
    cue.gates.push_back({"rate", rate, TOUCHDOWN_RATE, "deg/s", rate < TOUCHDOWN_RATE});
    return cue;
}

Cue kill_relative(const World& world, double a_max) {
    Cue cue;
    // The relative frame is the one the ship is flying in: the sector co-orbits its anchor and
    // every contact is fixed in it, so the relative vector is the ship's own velocity.
    const glm::dvec2 velocity = zone_velocity(world);
    const double speed = glm::length(velocity);
    if (!(speed > 1.0e-6)) return cue;  // nothing to kill: no mark where there is no data
    cue.active = true;
    cue.heading = bearing(-velocity);
    cue.dv_remaining = speed;
    cue.throttle = clamp01(speed / (a_max * THROTTLE_HORIZON));
    cue.burn_for = a_max > 0.0 ? speed / a_max : 0.0;
    cue.step = "kill velocity";
    return cue;
}

Cue transfer(const World& world, const Conic& conic, double t, double a_max) {
    Cue cue;
    if (world.nodes.empty()) return cue;
    const orbit::Node& node = world.nodes.front();
    cue.active = true;
    cue.burn_in = node.t - t;
    cue.dv_remaining = std::hypot(node.prograde, node.radial);
    cue.burn_for = a_max > 0.0 ? cue.dv_remaining / a_max : 0.0;
    // The node fires itself (World::run_nodes), so the pilot's throttle is not its actuator. What
    // the panel is for here is the countdown and where the nose goes when it comes.
    cue.throttle = 0.0;
    if (conic.valid) {
        const orbit::State at_node = orbit::state_at(conic.elements, node.t);
        const double radial_length = glm::length(at_node.r);
        const double speed = glm::length(at_node.v);
        if (radial_length > 0.0 && speed > 0.0) {
            const glm::dvec2 burn = at_node.v * (node.prograde / speed) +
                                    at_node.r * (node.radial / radial_length);
            if (glm::length(burn) > 1.0e-9) cue.heading = bearing(burn);
        }
    }
    cue.step = cue.burn_in > 0.0 ? "warp to the node" : "node due";
    return cue;
}

}  // namespace

const char* program_name(Program program) {
    switch (program) {
        case Program::None: return "";
        case Program::Circularize: return "CIRCULARIZE";
        case Program::MatchOrbit: return "MATCH ORBIT";
        case Program::Approach: return "APPROACH";
        case Program::Dock: return "DOCK";
        case Program::Deorbit: return "DEORBIT";
        case Program::Land: return "LAND";
        case Program::KillRelative: return "KILL RELATIVE";
        case Program::Transfer: return "TRANSFER";
    }
    return "";
}

std::string program_target(Program program, const World& world) {
    switch (program) {
        case Program::Circularize:
        case Program::Deorbit:
        case Program::Land: {
            const Conic conic = conic_of(world, world.elapsed);
            if (!conic.valid) return "";
            // The descent rows are aimed at a pad; circularising is aimed at nothing narrower than
            // the body, so it names that instead.
            const Pad* pad = program == Program::Circularize ? nullptr : target_pad(*conic.body);
            return pad ? pad->name : conic.body->name;
        }
        case Program::MatchOrbit:
            if (world.anchor_body >= 0 &&
                world.anchor_body < static_cast<int>(world.system.bodies.size())) {
                return world.system.bodies[static_cast<size_t>(world.anchor_body)].name;
            }
            return "";
        case Program::Dock:
            if (world.target_port >= 0 &&
                world.target_port < static_cast<int>(world.station_ports.size())) {
                const size_t seat = static_cast<size_t>(world.anchor_body);
                const std::string hull = seat < world.system.bodies.size()
                                             ? world.system.bodies[seat].id
                                             : std::string();
                return hull + "." + world.station_ports[static_cast<size_t>(world.target_port)].id;
            }
            return "";
        case Program::Approach: {
            const std::vector<Contact> contacts = world.contacts();
            const int at = contact_index(contacts, world.target);
            return at >= 0 ? contacts[static_cast<size_t>(at)].name : "";
        }
        case Program::None:
        case Program::KillRelative:
        case Program::Transfer:
            break;
    }
    return "";
}

Cue evaluate(Program program, const World& world, Real t) {
    if (program == Program::None) return Cue{};
    const double a_max = max_acceleration(world);
    const Conic conic = conic_of(world, t);
    switch (program) {
        case Program::Circularize: return circularize(conic, t, a_max);
        case Program::MatchOrbit: return match_orbit(world, conic, t, a_max);
        case Program::Approach: return approach(world, a_max);
        case Program::Dock: return dock(world, a_max);
        case Program::Deorbit: return deorbit(conic, t, a_max);
        case Program::Land: return land(world, conic, a_max);
        case Program::KillRelative: return kill_relative(world, a_max);
        case Program::Transfer: return transfer(world, conic, t, a_max);
        case Program::None: break;
    }
    return Cue{};
}

}  // namespace opra
