// Hohmann transfers and the departure window. Circular-to-circular only by construction; the
// ship is always on a closed-form conic (E2), which is what makes "the next window" a readout
// rather than a search. Lambert covers the arbitrary endpoints - intercepting a rock or a moving
// station - and maneuver nodes are the planner's one primitive.
#pragma once

#include <vector>

#include <glm/glm.hpp>

#include "orbit/kepler.h"

namespace opra::orbit {

/** A Hohmann transfer between circular orbits r1 and r2 about mu. */
struct Hohmann {
    double a_transfer = 0;     // semi-major axis of the transfer ellipse, metres
    double dv1 = 0;            // departure burn along the velocity, m/s
    double dv2 = 0;            // arrival burn along the velocity, m/s
    double dv_total = 0;       // |dv1| + |dv2|: the propellant cost, both directions
    double transfer_time = 0;  // half the transfer ellipse's period, seconds
};

Hohmann hohmann(double r1, double r2, double mu);

/**
 * Lead angle the target needs at the departure burn, pi - n2 * transfer_time. Measured in the
 * direction of travel, so it is what the target must be ahead of the ship by.
 */
double phase_angle_required(double n2, double transfer_time);

/**
 * Seconds until the target again leads the ship by `phase_required`. Both angles are the target's
 * lead over the ship; the lead closes at the synodic rate n1 - n2, and the answer is wrapped into
 * [0, synodic period) so the map can print "window opens in ...". A negative lead is legal and
 * wraps the same way.
 */
double time_to_window(double phase_now, double phase_required, double n1, double n2);

/** 2 pi / |n1 - n2|; +inf when the two rates coincide, so the phase never repeats. */
double synodic_period(double n1, double n2);

/** The velocities a transfer between two points needs at each end. */
struct LambertSolution {
    glm::dvec2 v1{0.0};  // the burn at r1, m/s
    glm::dvec2 v2{0.0};  // the velocity on arrival at r2, m/s
    /** False when no single-revolution conic takes `dt` to reach r2: the caller keeps its current
     *  conic and says so rather than flying a fabricated one. */
    bool ok = false;
    int iterations = 0;
};

/**
 * Lambert's problem in the plane, single revolution, the direct (counter-clockwise) way:
 * universal variables with bisection on the free-flight parameter z. No singular cases, converges
 * in under 20 iterations for a `dt` between 0.1x and 10x the Hohmann time.
 */
LambertSolution lambert(const glm::dvec2 &r1, const glm::dvec2 &r2, double dt, double mu);

/**
 * One impulsive burn. The frame is the orbital one at the burn point: prograde along the velocity,
 * radial outward from the primary. There is no normal component: the gameplay plane is 2D (E2).
 */
struct Node {
    double t = 0.0;         // seconds on the system clock
    double prograde = 0.0;  // m/s along the velocity
    double radial = 0.0;    // m/s outward from the primary
};

/** Propagates to the node, adds the delta-v, and converts back to elements. */
Elements apply_node(const Elements &before, const Node &node);

/** Applies a node list in the order given. The caller keeps the list sorted by time. */
Elements apply_nodes(const Elements &start, const std::vector<Node> &nodes);

/** The predicted state at `t` after a node list: the planner's drawn trajectory. */
State state_after(const Elements &start, const std::vector<Node> &nodes, double t);

}  // namespace opra::orbit
