#include "orbit/encounter.h"

#include <algorithm>
#include <cmath>
#include <limits>

#include "sim/system.h"

namespace opra::orbit {
namespace {

constexpr double kTwoPi = 6.28318530717958647692;

/** The plan's sampling resolution: one hundredth of the ship's period per step. */
constexpr int kSamplesPerPeriod = 100;

/** Bisection on the sign change, run to the double's own resolution in the bracket. */
constexpr int kBisectionSteps = 64;

/** A long coast is drawn coarse: past this many vertices the chart cannot show the difference. */
constexpr int kMaxPolyline = 256;

/** The body the ship's conic is about: the one carrying its mu, or the star at the root. */
int conic_primary(const SystemDef &system, const Elements &conic) {
    for (size_t i = 0; i < system.bodies.size(); ++i) {
        if (system.bodies[i].mu == conic.mu) return static_cast<int>(i);
    }
    return system.bodies.empty() ? -1 : 0;
}

/**
 * The characteristic time the search samples at: 2 pi over the mean motion. That is the period for
 * a closed conic, and for the escaping branch it is the same timescale continued - a hyperbola has
 * no period, and a search still has to take steps of some size.
 */
double sample_span(const Elements &conic) {
    const double motion = mean_motion(conic);
    return motion > 0.0 ? kTwoPi / motion : 0.0;
}

/** Body states at `t`: the caller's own snapshot when the time is the epoch it was taken at. */
const std::vector<BodyState> &states_at(const SystemDef &system, double t,
                                        const std::vector<BodyState> &snapshot, double epoch,
                                        std::vector<BodyState> &scratch) {
    if (t == epoch && snapshot.size() == system.bodies.size()) return snapshot;
    propagate(system, t, scratch);
    return scratch;
}

/** The ship's position in the system frame: its primary's own place plus the conic's offset. */
glm::dvec2 ship_position(const Elements &conic, const std::vector<BodyState> &states, int primary,
                         double t) {
    return states[static_cast<size_t>(primary)].position + position_at(conic, t);
}

/** Every candidate sphere's gap at one time: negative inside the sphere, infinite for a non-child. */
void gaps_at(const SystemDef &system, const Elements &conic, int primary, double time,
             const std::vector<BodyState> &snapshot, double epoch, std::vector<BodyState> &scratch,
             std::vector<double> &out) {
    const std::vector<BodyState> &states = states_at(system, time, snapshot, epoch, scratch);
    const glm::dvec2 ship = ship_position(conic, states, primary, time);
    out.assign(system.bodies.size(), std::numeric_limits<double>::infinity());
    for (size_t i = 0; i < system.bodies.size(); ++i) {
        const Body &body = system.bodies[i];
        if (body.soi <= 0.0 || body.parent != primary) continue;
        out[i] = glm::length(ship - states[i].position) - body.soi;
    }
}

/** The first time in [lo, hi] where `body`'s sphere is entered, to the bisection's resolution. */
double bisect_entry(const SystemDef &system, const Elements &conic, int primary,
                    const std::vector<BodyState> &snapshot, double epoch, int body, double lo,
                    double hi) {
    std::vector<BodyState> scratch;
    for (int i = 0; i < kBisectionSteps; ++i) {
        const double mid = 0.5 * (lo + hi);
        const std::vector<BodyState> &states = states_at(system, mid, snapshot, epoch, scratch);
        const double gap = glm::length(ship_position(conic, states, primary, mid) -
                                       states[static_cast<size_t>(body)].position) -
                           system.bodies[static_cast<size_t>(body)].soi;
        if (gap > 0.0) lo = mid;
        else hi = mid;
    }
    return hi;
}

struct Crossing {
    bool found = false;
    int body = -1;
    double time = 0.0;
};

/** The first sphere entry on the conic at or after `from`, across every body that orbits primary. */
Crossing first_crossing(const SystemDef &system, const Elements &conic, int primary,
                        const std::vector<BodyState> &snapshot, double epoch, double from) {
    Crossing out;
    const double span = sample_span(conic);
    if (!(span > 0.0)) return out;
    const double step = span / static_cast<double>(kSamplesPerPeriod);

    std::vector<BodyState> scratch;
    std::vector<double> previous;
    std::vector<double> current;
    double previous_time = from;
    gaps_at(system, conic, primary, previous_time, snapshot, epoch, scratch, previous);
    for (int k = 1; k <= kSamplesPerPeriod; ++k) {
        const double time = from + step * static_cast<double>(k);
        gaps_at(system, conic, primary, time, snapshot, epoch, scratch, current);
        double earliest = std::numeric_limits<double>::infinity();
        int body = -1;
        for (size_t i = 0; i < current.size(); ++i) {
            if (previous[i] > 0.0 && current[i] <= 0.0) {
                const double entry = bisect_entry(system, conic, primary, snapshot, epoch,
                                                  static_cast<int>(i), previous_time, time);
                if (entry < earliest) {
                    earliest = entry;
                    body = static_cast<int>(i);
                }
            }
        }
        if (body >= 0) {
            out.found = true;
            out.body = body;
            out.time = earliest;
            return out;
        }
        previous.swap(current);
        previous_time = time;
    }
    return out;
}

struct Flyby {
    bool ok = false;
    double time = 0.0;   // the sphere exit
    Elements after{};    // the conic that leaves the encounter, about the primary
};

/**
 * The body-relative swing (plan 3.6): convert the entry state into the body's frame, derive the
 * conic there, find where it leaves the sphere, and convert that state back. Nothing is integrated;
 * the assist falls out of the frame change, which is the whole of patched conics.
 */
Flyby flyby(const SystemDef &system, const Elements &conic, int primary, const Crossing &cross,
            const std::vector<BodyState> &snapshot, double epoch) {
    Flyby out;
    const Body &body = system.bodies[static_cast<size_t>(cross.body)];
    if (!(body.mu > 0.0)) return out;

    std::vector<BodyState> scratch;
    const std::vector<BodyState> entry = states_at(system, cross.time, snapshot, epoch, scratch);
    const glm::dvec2 ship_r = ship_position(conic, entry, primary, cross.time);
    const glm::dvec2 ship_v =
        entry[static_cast<size_t>(primary)].velocity + velocity_at(conic, cross.time);
    const glm::dvec2 body_r = entry[static_cast<size_t>(cross.body)].position;
    const glm::dvec2 body_v = entry[static_cast<size_t>(cross.body)].velocity;
    const glm::dvec2 rel_r = ship_r - body_r;
    const glm::dvec2 rel_v = ship_v - body_v;
    const Elements hyper = from_state(rel_r, rel_v, body.mu, cross.time);
    if (is_elliptic(hyper) || !(hyper.a != 0.0)) return out;  // captured: there is no exit

    // The exit, exactly. In the body's frame the sphere is a fixed circle, so the hyperbola leaves
    // it at the true anomaly mirrored about periapsis, and the hyperbolic Kepler equation gives
    // that time as one subtraction off the entry's own mean anomaly. No second walk: a walk would
    // have to sample the whole fly-by, which is far longer than the hyperbola's own timescale.
    const glm::dvec2 eccentricity(hyper.e * std::cos(hyper.omega),
                                  hyper.e * std::sin(hyper.omega));
    const double nu = std::atan2(eccentricity.x * rel_r.y - eccentricity.y * rel_r.x,
                                 eccentricity.x * rel_r.x + eccentricity.y * rel_r.y);
    const double ratio = std::sqrt((hyper.e - 1.0) / (hyper.e + 1.0)) * std::tan(0.5 * nu);
    const double hyperbolic_anomaly = 2.0 * std::atanh(ratio);
    const double mean = hyper.e * std::sinh(hyperbolic_anomaly) - hyperbolic_anomaly;
    const double motion = mean_motion(hyper);
    if (!(motion > 0.0)) return out;
    const double exit_time = cross.time - 2.0 * mean / motion;
    if (!std::isfinite(exit_time) || !(exit_time > cross.time)) return out;

    std::vector<BodyState> exit_scratch;
    const std::vector<BodyState> at_exit = states_at(system, exit_time, snapshot, epoch, exit_scratch);
    const glm::dvec2 exit_r =
        at_exit[static_cast<size_t>(cross.body)].position + position_at(hyper, exit_time);
    const glm::dvec2 exit_v =
        at_exit[static_cast<size_t>(cross.body)].velocity + velocity_at(hyper, exit_time);
    out.after = from_state(exit_r - at_exit[static_cast<size_t>(primary)].position,
                           exit_v - at_exit[static_cast<size_t>(primary)].velocity,
                           system.bodies[static_cast<size_t>(primary)].mu, exit_time);
    out.time = exit_time;
    out.ok = true;
    return out;
}

/** The coast from `from` to `to`, sampled at the search's own resolution. */
void append_coast(std::vector<glm::dvec2> &points, const SystemDef &system, const Elements &conic,
                  int primary, const std::vector<BodyState> &snapshot, double epoch, double from,
                  double to) {
    const double span = sample_span(conic);
    const double step = span > 0.0 ? span / static_cast<double>(kSamplesPerPeriod) : to - from;
    int count = step > 0.0 ? static_cast<int>(std::lround((to - from) / step)) : 1;
    count = std::max(1, std::min(count, kMaxPolyline));
    std::vector<BodyState> scratch;
    for (int i = 0; i <= count; ++i) {
        const double time =
            from + (to - from) * static_cast<double>(i) / static_cast<double>(count);
        const std::vector<BodyState> &states = states_at(system, time, snapshot, epoch, scratch);
        points.push_back(ship_position(conic, states, primary, time));
    }
}

}  // namespace

std::vector<EncounterLeg> predict_encounters(const SystemDef &system,
                                             const std::vector<BodyState> &bodies,
                                             const Elements &ship_conic, double t, int depth) {
    std::vector<EncounterLeg> legs;
    if (depth <= 0 || system.bodies.empty() || !(ship_conic.mu > 0.0) || ship_conic.a == 0.0) {
        return legs;
    }
    const int primary = conic_primary(system, ship_conic);
    if (primary < 0) return legs;

    Elements conic = ship_conic;
    double start = t;
    bool captured = false;
    for (int n = 0; n < depth; ++n) {
        const Crossing cross = first_crossing(system, conic, primary, bodies, t, start);
        if (!cross.found) break;

        EncounterLeg leg;
        leg.body = cross.body;
        leg.post = n > 0;
        leg.elements = conic;
        append_coast(leg.points, system, conic, primary, bodies, t, start, cross.time);
        legs.push_back(std::move(leg));

        const Flyby swing = flyby(system, conic, primary, cross, bodies, t);
        if (!swing.ok) {
            captured = true;
            break;
        }
        conic = swing.after;
        start = swing.time;
    }
    if (legs.empty() || captured) return legs;

    // The final coast, marked by body == -1: where the last post-encounter conic goes.
    EncounterLeg tail;
    tail.body = -1;
    tail.post = true;
    tail.elements = conic;
    const double span = sample_span(conic);
    if (span > 0.0) append_coast(tail.points, system, conic, primary, bodies, t, start, start + span);
    legs.push_back(std::move(tail));
    return legs;
}

}  // namespace opra::orbit
