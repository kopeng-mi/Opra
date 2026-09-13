import io

p = 'src/sim/physics.h'
s = open(p, encoding='utf-8').read()
old = """struct ShipSpec {
    const char *name;
    const char *role;
    Real mass;
    Real thrust;
    Real fuel;
    Real torque;
    Real hull;
    Real length;
    Real cargo;
    /** Heat shed per second. */
    Real cooling;
    Real scanScale;
    Real collectScale;
};"""
new = """struct ShipSpec {
    const char *name;
    const char *role;
    Real mass;
    Real thrust;
    Real fuel;
    Real torque;
    Real hull;
    Real length;
    Real cargo;
    /** Heat shed per second. */
    Real cooling;
    Real scanScale;
    Real collectScale;
    /**
     * Lateral authority as a fraction of main thrust (plan 05 s5.1): close quarters is won with
     * the RCS, so the fraction is a derived value - the component library's RCS blocks decide it
     * (s6.2's derivation fills it in derive_spec), and the stock table carries the figures the
     * derivation lands on.
     */
    Real strafeFraction = 0.22;
};"""
assert s.count(old) == 1, "physics.h spec"
s = s.replace(old, new)
open(p, 'w', encoding='utf-8', newline='\n').write(s)
print('physics.h ok')

p = 'src/sim/physics.cpp'
s = open(p, encoding='utf-8').read()
old = """const ShipSpec SHIPS[3] = {
    {"Kestrel", "Independent corvette", 82000, 1600000, 16000, 1.35, 100, 42, 120, 0.055, 1, 1},
    {"Mule", "Heavy salvage tug", 142000, 1950000, 30000, 0.82, 150, 58, 320, 0.075, 1, 1},
    {"Needle", "Fast reconnaissance cutter", 43000, 1200000, 10000, 2.05, 75, 31, 40, 0.05, 1, 1},
};"""
new = """const ShipSpec SHIPS[3] = {
    {"Kestrel", "Independent corvette", 82000, 1600000, 16000, 1.35, 100, 42, 120, 0.055, 1, 1,
     0.22},
    {"Mule", "Heavy salvage tug", 142000, 1950000, 30000, 0.82, 150, 58, 320, 0.075, 1, 1, 0.19},
    {"Needle", "Fast reconnaissance cutter", 43000, 1200000, 10000, 2.05, 75, 31, 40, 0.05, 1, 1,
     0.28},
};"""
assert s.count(old) == 1, "ships table"
s = s.replace(old, new)

old = """    Real ax = forward.x * thrust * maxAcceleration + right.x * strafe * maxAcceleration * 0.22;
    Real ay = forward.y * thrust * maxAcceleration + right.y * strafe * maxAcceleration * 0.22;"""
new = """    // s5.1: translation authority is the hull's own derived fraction, not a global constant - a
    // refit with more RCS blocks manoeuvres better.
    const Real strafe_fraction = spec.strafeFraction;
    Real ax = forward.x * thrust * maxAcceleration +
              right.x * strafe * maxAcceleration * strafe_fraction;
    Real ay = forward.y * thrust * maxAcceleration +
              right.y * strafe * maxAcceleration * strafe_fraction;"""
assert s.count(old) == 1, "strafe line"
s = s.replace(old, new)

old = """    const Real closing = state.velocity.x * nx + state.velocity.y * ny;
    if (closing >= 0) return 0;
    state.velocity.x -= restitution * closing * nx;
    state.velocity.y -= restitution * closing * ny;
    // Damage starts at 6 m/s and climbs slower than the closing speed, and a fresh impact costs hull
    // while grinding along a boulder does not."""
new = """    const Real closing = state.velocity.x * nx + state.velocity.y * ny;
    if (closing >= 0) return 0;
    // s5.1: at close quarters a nudge against a rock is a nudge. Restitution fades out below 5
    // m/s of closing speed, so low-speed contact cannot launch the hull.
    restitution *= std::clamp(-closing / 5.0, 0.0, 1.0);
    state.velocity.x -= restitution * closing * nx;
    state.velocity.y -= restitution * closing * ny;
    // Damage starts at 6 m/s and climbs slower than the closing speed, and a fresh impact costs hull
    // while grinding along a boulder does not."""
assert s.count(old) == 1, "restitution"
s = s.replace(old, new)
open(p, 'w', encoding='utf-8', newline='\n').write(s)
print('physics.cpp ok')
