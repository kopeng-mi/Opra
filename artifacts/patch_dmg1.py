p = 'src/sim/combat.cpp'
s = open(p, encoding='utf-8').read()
old = """Real kinetic_damage(Real mass, Real relative_speed) {
    const Real energy = 0.5 * mass * relative_speed * relative_speed;
    return energy * DAMAGE_K;
}"""
new = """Real kinetic_damage(Real mass, Real relative_speed) {
    const Real energy = 0.5 * mass * relative_speed * relative_speed;
    return energy * DAMAGE_K;
}

bool shape_is_drive(const Shape &shape, const Collider &collider) {
    // The drive pods sit aft of the hull's mid-length; the tanks and the hab sit forward. The
    // classification is geometric, so a refit's own shape set classifies itself (s6.2).
    Real half_length = 1.0;
    for (const Shape &other : collider.shapes) {
        half_length = std::max<Real>(half_length, std::abs(other.local_pos.y) +
                                                      (other.kind == Shape::Kind::Circle
                                                           ? other.radius
                                                           : other.half_length));
    }
    return shape.local_pos.y < -half_length * 0.15;
}

Real apply_ship_damage(ShipState &ship, int shape, Real energy) {
    // `energy` arrives already converted to hull points by the caller; the kinetic conversion is
    // kinetic_damage's, and this function is where s5.4's per-shape rule lives.
    const Real dealt = std::max(0.0, energy);
    ship.hull = std::max<Real>(0, ship.hull - dealt);
    ship.contactTimer = 0;
    if (shape < 0 || shape >= static_cast<int>(ship.collider.shapes.size())) return dealt;
    const Shape &hit = ship.collider.shapes[static_cast<size_t>(shape)];
    // s5.4: a hit on a drive pod degrades thrust; a hit on a tank vents propellant - and keeps
    // venting, which is what makes the hit matter after the round is gone.
    if (shape_is_drive(hit, ship.collider)) {
        ship.thrustDamage = std::min<Real>(0.85, ship.thrustDamage + dealt * 0.004);
    } else {
        const Real vented = std::min(ship.fuel, dealt * 0.8);
        ship.fuel -= vented;
        ship.fuelLeak += dealt * 0.35;
    }
    return dealt;
}"""
assert s.count(old) == 1, 'apply fn'
s = s.replace(old, new)
open(p, 'w', encoding='utf-8', newline='\n').write(s)
print('combat.cpp ok')
