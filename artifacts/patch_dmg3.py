p = 'src/sim/world.cpp'
s = open(p, encoding='utf-8').read()

# rounds: per-shape damage on the ship
old = """        // PDC vs rock: the same damage path the cutter drives (s5.5).
                Obstacle &hit = *const_cast<Obstacle *>(body);
                hit.hp -= round.damage;
                if (hit.hp <= 0) break_rock(hit);
                spent = true;
                break;
            }
        }
        if (spent) rounds.erase(rounds.begin() + static_cast<long>(i));"""
new = """        // PDC vs rock: the same damage path the cutter drives (s5.5).
                Obstacle &hit = *const_cast<Obstacle *>(body);
                hit.hp -= round.damage;
                if (hit.hp <= 0) break_rock(hit);
                spent = true;
                break;
            }
        }
        // s5.4: a round that crosses the ship's own hull lands on the compound shape it struck -
        // drive pod or tank, not a single pool. A mount's own fire spawns clear of the hull and is
        // occlusion-tested before firing, so this path is everything else.
        if (!spent) {
            const int shape = swept_ship_shape(ship.collider, ship.position, ship.angle, from, to);
            if (shape >= 0) {
                apply_ship_damage(ship, shape, round.damage);
                spent = true;
            }
        }
        if (spent) rounds.erase(rounds.begin() + static_cast<long>(i));"""
assert s.count(old) == 1, 'round sweep'
s = s.replace(old, new)

# torpedoes: warhead on the struck shape
old = """        if (detonated || torpedo.age > ROUND_LIFETIME) {
            torpedoes.erase(torpedoes.begin() + static_cast<long>(i));
        }"""
new = """        // The warhead on the ship's own hull: the compound shape it struck takes it (s5.4) - the
        // kinetic term of a 450 kg torpedo at 320 m/s plus the fixed warhead.
        const int ship_shape = swept_ship_shape(ship.collider, ship.position, ship.angle, from, to);
        if (ship_shape >= 0) {
            apply_ship_damage(ship, ship_shape, kinetic_damage(450.0, 320.0) + torpedo.warhead);
            detonated = true;
        }
        if (detonated || torpedo.age > ROUND_LIFETIME) {
            torpedoes.erase(torpedoes.begin() + static_cast<long>(i));
        }"""
assert s.count(old) == 1, 'torpedo sweep'
s = s.replace(old, new)
open(p, 'w', encoding='utf-8', newline='\n').write(s)
print('world.cpp sweeps ok')
