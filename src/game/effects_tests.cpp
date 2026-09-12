// Assert suite for the effect pool: recycling under a burst larger than the pool, the shape of the
// lifetime curve at both of its ends, the mapping from a contact's speed to its burst, and the
// three world edges `update` emits on. Every number below is derived in the comment beside it from
// the constants in game/effects.h and the formulas in sim/physics.cpp - none is captured from a run.
#include "game/effects_tests.h"

#include <algorithm>
#include <cmath>

#include "game/effects.h"
#include "selftest.h"
#include "sim/world.h"

namespace opra {
namespace {

using selftest::check;
using selftest::check_close;

void test_pool_reuse() {
    Effects pool(16);
    check(pool.capacity() == 16 && pool.size() == 0, "effects: a fresh pool is empty");

    // A pool with no slots is the one case in which a spawn is refused: nothing to recycle.
    Effects none(0);
    check(!none.spawn(Effect{}), "effects: a pool with no slots refuses every spawn");

    // 100 bursts of 12 sparks into 16 slots: 1200 particles through a fixed array. Nothing grows,
    // nothing allocates, and the live count cannot pass the capacity.
    for (int burst = 0; burst < 100; ++burst) {
        const Real born = static_cast<Real>(burst) * 0.001;
        emit_impact(pool, Vec2{static_cast<Real>(burst), 0.0}, Vec2{1.0, 0.0}, 40.0, born);
    }
    check(pool.capacity() == 16, "effects: the pool's capacity never changes");
    check(pool.size() == 16, "effects: a full pool holds exactly its capacity");
    check(pool.live(0.2) <= 16, "effects: the live count never exceeds the capacity");
    check(pool.live(0.2) == 16, "effects: 0.102 s of lifetime keeps every slot live at 0.2 s");

    // The oldest are what got recycled: the survivor is the newest burst, and nothing from the
    // first 98 bursts is still in the pool.
    Real oldest = 1e9;
    Real newest = -1e9;
    for (int slot = 0; slot < pool.size(); ++slot) {
        oldest = std::min(oldest, pool.at(slot).born);
        newest = std::max(newest, pool.at(slot).born);
    }
    check_close(newest, 0.099, 1e-12, "effects: the newest burst is still in the pool");
    check(oldest >= 0.098 - 1e-9, "effects: and the oldest are the ones that made room for it");

    pool.clear();
    check(pool.size() == 0 && pool.capacity() == 16, "effects: clear drops the particles, not the pool");
}

void test_lifetime() {
    Effect spark;
    spark.kind = EffectKind::Spark;
    spark.origin = {12.0, -4.0};
    spark.dir = {1.0, 0.0};
    spark.speed = 20.0;
    spark.size = 2.0;
    spark.strength = 1.0;
    spark.born = 7.5;
    spark.duration = fx::IMPACT_SECONDS;

    check_close(effect_fade(spark, spark.born), 1.0, 1e-12, "effects: full strength at t = 0");
    check_close(effect_fade(spark, spark.born + spark.duration), 0.0, 0.0,
                "effects: invisible at t = duration");
    check_close(effect_fade(spark, spark.born + spark.duration * 2.0), 0.0, 0.0,
                "effects: and invisible past it");
    // The curve is (1 - s)^2, so its quarter points are 0.5625 and 0.0625 exactly.
    check_close(effect_fade(spark, spark.born + spark.duration * 0.25), 0.5625, 1e-12,
                "effects: a quarter of the way in it is still bright");
    check_close(effect_fade(spark, spark.born + spark.duration * 0.75), 0.0625, 1e-12,
                "effects: three quarters in it is nearly out");
    check_close(effect_intensity(spark, spark.born), 1.0, 1e-12,
                "effects: intensity is the payload strength on the curve");

    Real previous = 1.0;
    for (int step = 0; step <= 40; ++step) {
        const Real fade = effect_fade(spark, spark.born + spark.duration * static_cast<Real>(step) / 40.0);
        check(fade <= previous + 1e-12, "effects: the lifetime curve never rises");
        previous = fade;
    }
    check(previous <= 0.0, "effects: and it reaches zero at the end of life");

    // A clock behind the emission is a fresh run, not an effect that is impossibly old: it retires.
    check_close(effect_fade(spark, spark.born - 1.0), 0.0, 0.0,
                "effects: a reset clock retires the burst");

    // Motion is the particle's own velocity times its own age, from the point it was born at.
    const Vec2 born_at = effect_position(spark, spark.born);
    check_close(born_at.x, 12.0, 1e-12, "effects: it starts at the contact point (x)");
    check_close(born_at.y, -4.0, 1e-12, "effects: it starts at the contact point (y)");
    const Vec2 moved = effect_position(spark, spark.born + 0.1);
    check_close(moved.x, 14.0, 1e-12, "effects: 20 m/s for 0.1 s is two metres");
    check_close(moved.y, -4.0, 1e-12, "effects: and nothing across its own direction");
}

void test_magnitude() {
    check(spark_count(40.0) > spark_count(4.0), "effects: 40 m/s throws a bigger burst than 4 m/s");
    check(spark_count(4.0) == fx::SPARK_MIN, "effects: a light contact is at the floor of the burst");
    check(spark_count(1.0e4) == fx::SPARK_MAX, "effects: and a catastrophe is at its ceiling");
    check_close(impact_strength(fx::CONTACT_FLOOR), 0.0, 1e-12,
                "effects: the sim's own damage floor is zero strength");
    check_close(impact_strength(fx::SPARK_FULL_SPEED), 1.0, 1e-12,
                "effects: 40 m/s is full strength");

    Real previous_count = -1.0;
    Real previous_strength = -1.0;
    for (int i = 0; i <= 200; ++i) {
        const Real speed = static_cast<Real>(i);
        const Real count = static_cast<Real>(spark_count(speed));
        const Real strength = impact_strength(speed);
        check(count >= previous_count, "effects: the burst count is monotone in the speed");
        check(strength >= previous_strength, "effects: the brightness is monotone in the speed");
        previous_count = count;
        previous_strength = strength;
    }

    // The same statement through the emitter, which is what the world actually calls.
    Effects slow(32);
    Effects fast(32);
    const int few = emit_impact(slow, Vec2{0.0, 0.0}, Vec2{0.0, 1.0}, 4.0, 0.0);
    const int many = emit_impact(fast, Vec2{0.0, 0.0}, Vec2{0.0, 1.0}, 40.0, 0.0);
    check(many > few, "effects: the stronger contact fills more of the pool");
    check(few == spark_count(4.0), "effects: the burst is the count the mapping promised");
}

void test_world_edges() {
    World world;
    world.elapsed = 10.0;
    world.ship.angle = 0.0;
    world.ship.position = {0.0, 0.0};
    world.ship.velocity = {0.0, 0.0};
    world.ship.hull = 100.0;
    world.ship.contactTimer = CONTACT_REARM;  // re-armed: the next contact counts as fresh
    world.docked = false;
    world.fragments.clear();

    Effects pool(64);
    pool.update(world);
    check(pool.size() == 0, "effects: the first frame of a run is quiet, whatever shape it is in");

    // A 44 m/s contact. physics.cpp charges (44 - 6) * 0.55 = 20.9 hull for it and leaves the hull
    // at 1.3x the closing speed, along the contact normal - here the ship's starboard, angle 0.
    world.elapsed = 10.016;
    world.ship.contactTimer = 0.0;  // only a damaging contact clears this
    world.ship.hull = 100.0 - 20.9;
    world.ship.velocity = {1.3 * 44.0, 0.0};
    pool.update(world);
    // (44 - 6) / 4 = 9 sparks past the two the floor always throws.
    check(pool.size() == 11, "effects: a 44 m/s contact throws eleven sparks");
    check(pool.live(10.016) == 11, "effects: every one of them is live the moment it lands");
    bool forward = true;
    bool on_hull = true;
    for (int slot = 0; slot < pool.size(); ++slot) {
        const Effect &spark = pool.at(slot);
        forward = forward && spark.dir.x > 0.0;
        on_hull = on_hull && spark.origin.x <= 1e-9 &&
                  spark.origin.x >= -world.ship.bounds.halfWidth - 1e-9;
        check_close(spark.born, 10.016, 1e-12, "effects: the burst is born on the sim clock");
    }
    check(forward, "effects: the sparks leave along the contact normal");
    check(on_hull, "effects: and they start on the hull's own silhouette");

    // A second contact inside the re-arm window is one the sim never charged for, so it makes no
    // sparks: the edge, not the overlap, is the event.
    world.elapsed = 10.05;
    pool.update(world);
    check(pool.size() == 11, "effects: a contact inside the re-arm window adds nothing");

    // A fracture. break_rock retires the body in place and files its pieces on the tail of the
    // fragment deque. A rock too small to fracture into pieces retires with nothing else changed,
    // which is why the retirement, not the pieces, is the edge.
    world.elapsed = 10.2;
    world.ship.contactTimer = CONTACT_REARM;
    const Obstacle &broken = world.rocks[7];
    const Real broken_radius = broken.radius;
    world.rocks[7].retired = true;  // exactly what break_rock leaves behind
    world.rocks[7].hp = 0.0;
    pool.update(world);
    check(pool.size() == 11 + fx::DUST_COUNT,
          "effects: a break with no pieces still puffs");
    Vec2 dust_centre{};
    Real dust_size = 0.0;
    int dust = 0;
    for (int slot = 0; slot < pool.size(); ++slot) {
        const Effect &effect = pool.at(slot);
        if (effect.kind != EffectKind::Dust) continue;
        dust_centre.x += effect.origin.x;
        dust_centre.y += effect.origin.y;
        dust_size += effect.size;
        ++dust;
    }
    check(dust == fx::DUST_COUNT, "effects: the puff is the whole spray");
    // A grain is born on a ring about the body, and a ring is where the body is: this is the check
    // that the burst is placed in world metres and not in anything ship-relative.
    const Real ring = std::max(broken_radius, fx::DUST_MIN_RADIUS) * fx::DUST_BIRTH_RING;
    bool on_ring = true;
    for (int slot = 0; slot < pool.size(); ++slot) {
        const Effect &effect = pool.at(slot);
        if (effect.kind != EffectKind::Dust) continue;
        const Real off = std::hypot(effect.origin.x - broken.x, effect.origin.y - broken.y);
        on_ring = on_ring && std::abs(off - ring) < 1e-9;
    }
    check(on_ring, "effects: every grain is born on the ring about the body");
    check_close(dust_centre.x / dust, broken.x, ring, "effects: the puff sits on the body (x)");
    check_close(dust_centre.y / dust, broken.y, ring, "effects: the puff sits on the body (y)");
    const Real mean_grain = dust_size / dust;
    // Grains are 0.18 of the puff, jittered 0.6..1.4x - and the puff has a floor, because a body
    // small enough to be a few pixels must still break as something the pilot can see.
    const Real grain = std::max(broken_radius, fx::DUST_MIN_RADIUS) * fx::DUST_SIZE;
    check(mean_grain > grain * 0.7 && mean_grain < grain * 1.4,
          "effects: the grains are sized off the body's own radius");
    pool.update(world);
    check(pool.size() == 11 + fx::DUST_COUNT, "effects: and it puffs once, not every frame");

    // The pieces of that break, arriving on the tail of the deque: one trail row per piece.
    Obstacle a;
    a.x = 100.0;
    a.y = 0.0;
    a.radius = 28.284;
    a.vx = 0.0;
    a.vy = 20.0;
    Obstacle b = a;
    b.y = 4.0;
    b.vy = -20.0;
    world.elapsed = 10.3;
    world.fragments.push_back(a);
    world.fragments.push_back(b);
    pool.update(world);
    check(pool.size() == 11 + fx::DUST_COUNT + 2 * fx::TRAIL_CUBES,
          "effects: the pieces leave a trail each");
    int trails = 0;
    bool trails_ride = true;
    for (int slot = 0; slot < pool.size(); ++slot) {
        const Effect &effect = pool.at(slot);
        if (effect.kind != EffectKind::Trail) continue;
        trails_ride = trails_ride && std::abs(effect.speed - 20.0) < 1e-9 &&
                      std::abs(effect.origin.x - 100.0) < 1e-9;
        ++trails;
    }
    check(trails == 2 * fx::TRAIL_CUBES, "effects: the trail is one row per piece");
    check(trails_ride, "effects: every trail cube rides with its own piece");

    // A capture: the rising edge is the pulse, and the pulse is once.
    world.elapsed = 11.0;
    world.docked = true;
    pool.update(world);
    const int after_capture = pool.size();
    check(after_capture == 11 + fx::DUST_COUNT + 2 * fx::TRAIL_CUBES + fx::PULSE_COUNT,
          "effects: a capture pulses once");
    pool.update(world);
    check(pool.size() == after_capture, "effects: and stays quiet while docked");

    // Nothing in the pool is live half a second later: the longest life here is the 0.8 s trails
    // from 10.3, so the 11.0 pulse is the only thing in its last half second.
    check(pool.live(11.0) == fx::PULSE_COUNT + 2 * fx::TRAIL_CUBES,
          "effects: the fresh pulse and the trails are all that is still live");
    check(pool.live(11.0 + fx::DOCK_SECONDS) == 0, "effects: and everything is gone by 11.6 s");

    // A fresh run: the clock goes back to zero and every edge the pool remembers is forgotten, so
    // the same broken rock cannot puff twice.
    World fresh_world;
    fresh_world.elapsed = 0.0;
    pool.update(fresh_world);
    check(pool.size() == 0, "effects: a run reset clears the glass");
}

}  // namespace

int effects_tests() {
    const int before = selftest::failures();
    test_pool_reuse();
    test_lifetime();
    test_magnitude();
    test_world_edges();
    return selftest::failures() - before;
}

}  // namespace opra
