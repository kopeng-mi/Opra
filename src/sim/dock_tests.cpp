// Assert suite for the docking layer: every gate term at its boundary, the continuous hold, and the
// capture spline's end conditions.
#include "sim/dock_tests.h"

#include <cmath>

#include "selftest.h"
#include "sim/dock.h"

namespace opra {
namespace {

using selftest::check;
using selftest::check_close;

constexpr double kDeg = 3.14159265358979323846 / 180.0;

/** A target port at the origin facing +x, which makes the corridor frame the x axis. */
PortState target_port() { return PortState{{0.0, 0.0}, {1.0, 0.0}}; }

/** A ship port approaching down that corridor: axial along +x, offset laterally in y. */
PortState ship_port(double axial, double lateral, double alignment) {
    const double angle = 3.14159265358979323846 + alignment;  // facing back down the corridor
    PortState port;
    port.position = {axial, lateral};
    port.normal = {std::cos(angle), std::sin(angle)};
    return port;
}

ApproachGate gate_for(const PortState &ship, double closing, double spin, double held_for = 0.0,
                      double dt = 0.25) {
    // The ship sits outside the port along its outward normal and closes by moving back down it.
    const Vec2 ship_velocity{-closing, 0.0};
    return evaluate_gate(ship, ship_velocity, target_port(), Vec2{0.0, 0.0}, spin, held_for, dt);
}

void test_gate_terms() {
    const ApproachGate good = gate_for(ship_port(30.0, 2.0, 5.0 * kDeg), 0.8, 1.0 * kDeg);
    check(good.axial_ok && good.lateral_ok && good.closing_ok && good.alignment_ok && good.rate_ok,
          "dock: a clean approach passes every term");
    check(good.all_ok(), "dock: a clean approach is inside the gate");
    check_close(good.axial, 30.0, 1e-12, "dock: axial distance is measured down the corridor");
    check_close(good.lateral, 2.0, 1e-12, "dock: lateral offset is measured across it");
    check_close(good.closing, 0.8, 1e-12, "dock: closing rate is positive toward the port");
    check_close(good.alignment, 5.0 * kDeg, 1e-9, "dock: alignment is the angle off the corridor");

    // Each term fails on its own, so the HUD can point at the one that is wrong.
    const ApproachGate too_far = gate_for(ship_port(80.0, 0.0, 0.0), 0.5, 0.0);
    check(!too_far.axial_ok && too_far.lateral_ok && too_far.closing_ok,
          "dock: beyond the corridor only the axial term fails");
    const ApproachGate off_axis = gate_for(ship_port(30.0, 12.0, 0.0), 0.5, 0.0);
    check(!off_axis.lateral_ok && off_axis.axial_ok, "dock: 12 m off axis is out of tolerance");
    const ApproachGate separating = gate_for(ship_port(30.0, 0.0, 0.0), -0.4, 0.0);
    check(!separating.closing_ok && separating.axial_ok,
          "dock: moving away is not a closing rate, however small");
    const ApproachGate too_fast = gate_for(ship_port(30.0, 0.0, 0.0), 2.5, 0.0);
    check(!too_fast.closing_ok && too_fast.axial_ok, "dock: 2.5 m/s is too fast to capture");
    const ApproachGate crooked = gate_for(ship_port(30.0, 0.0, 20.0 * kDeg), 0.5, 0.0);
    check(!crooked.alignment_ok && crooked.closing_ok, "dock: 20 degrees off is out of tolerance");
    const ApproachGate spinning = gate_for(ship_port(30.0, 0.0, 0.0), 0.5, 4.0 * kDeg);
    check(!spinning.rate_ok && spinning.alignment_ok, "dock: 4 deg/s of relative spin is too much");
    const ApproachGate behind = gate_for(ship_port(-5.0, 0.0, 0.0), 0.5, 0.0);
    check(!behind.axial_ok, "dock: a port behind the target is not down the corridor");
}

void test_gate_hold() {
    // 0.4 s of holding, accumulated a frame at a time: one good frame is not a capture.
    const PortState ship = ship_port(30.0, 1.0, 2.0 * kDeg);
    ApproachGate gate = gate_for(ship, 0.6, 0.5 * kDeg, 0.0, 0.15);
    check(gate.all_ok() && !gate.held, "dock: one good frame is not a capture");
    check_close(gate.held_for, 0.15, 1e-12, "dock: the hold accumulates the frame's dt");
    gate = gate_for(ship, 0.6, 0.5 * kDeg, gate.held_for, 0.15);
    check(!gate.held, "dock: 0.30 s is still short of the 0.4 s hold");
    gate = gate_for(ship, 0.6, 0.5 * kDeg, gate.held_for, 0.15);
    check(gate.held, "dock: 0.45 s of a clean approach closes the gate");

    // A frame that breaks a term restarts the hold: the approach has to be clean throughout.
    ApproachGate broken = gate_for(ship_port(30.0, 20.0, 0.0), 0.6, 0.5 * kDeg, 0.35, 0.2);
    check(!broken.held && broken.held_for == 0.0, "dock: a failed term restarts the hold");
}

void test_capture() {
    Capture capture;
    capture.duration = 2.0;
    capture.from_offset = {12.0, -3.0};
    capture.from_velocity = {-6.0, 1.5};  // closing on the port at 6 m/s

    const Vec2 start = capture.offset_at(0.0);
    check_close(start.x, 12.0, 1e-12, "dock: the capture starts where the ship was (x)");
    check_close(start.y, -3.0, 1e-12, "dock: the capture starts where the ship was (y)");
    const Vec2 start_velocity = capture.velocity_at(0.0);
    check_close(start_velocity.x, -6.0, 1e-9, "dock: the capture starts at the closing speed");
    check_close(start_velocity.y, 1.5, 1e-9, "dock: and with the whole relative velocity");

    const Vec2 end = capture.offset_at(capture.duration);
    check_close(end.x, 0.0, 1e-12, "dock: the capture ends at hard dock");
    check_close(end.y, 0.0, 1e-12, "dock: the capture ends at hard dock on both axes");
    const Vec2 end_velocity = capture.velocity_at(capture.duration);
    check_close(end_velocity.x, 0.0, 1e-9, "dock: the capture ends at rest");
    check_close(end_velocity.y, 0.0, 1e-9, "dock: the capture ends at rest on both axes");

    // Monotone closure in x: a capture never backs the ship out of the corridor.
    double previous = capture.offset_at(0.0).x;
    bool monotone = true;
    for (int i = 1; i <= 20; ++i) {
        const double at = capture.offset_at(0.2 * i).x;
        if (at > previous) monotone = false;
        previous = at;
    }
    check(monotone, "dock: the capture closes monotonically");
}

void test_undock() {
    const PortState target{{100.0, 50.0}, {0.0, -1.0}};
    const Vec2 impulse = undock_impulse(target);
    check_close(impulse.x, 0.0, 1e-12, "dock: the undock impulse runs along the port's normal");
    check_close(impulse.y, -UNDOCK_SPEED, 1e-12, "dock: and away from the target at 0.5 m/s");
}

}  // namespace

int dock_tests() {
    const int before = selftest::failures();
    test_gate_terms();
    test_gate_hold();
    test_capture();
    test_undock();
    return selftest::failures() - before;
}

}  // namespace opra
