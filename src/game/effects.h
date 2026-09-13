// Transient effects: contact sparks, fracture dust and fragment trails, and the confirming pulse
// a capture puts on a port. A fixed ring of slots aged by the sim clock alone - an effect is its
// emission time and the current time, nothing else - so a burst allocates nothing, the pool never
// grows past its capacity, and the sim's RNG is never touched. The forms live here; `Effects::update`
// reads the world's own edges rather than the sim knowing a frame exists.
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

#include <glm/glm.hpp>

#include "core/units.h"
#include "sim/component.h"

namespace opra {

/** The world, for `update` only: the effect layer reads it, and it never reads back. */
struct World;

/** What a particle looks like, not what caused it. `update` maps the triggers onto these. */
enum class EffectKind : std::uint8_t {
    Spark = 0,  // an impact: a fan of cubes thrown back along the contact normal
    Dust = 1,   // a fracture: one grain of the radial puff
    Trail = 2,  // a fracture: a cube riding one fragment's own velocity
    Pulse = 3,  // a capture: one cube of the ring on the port
};

/**
 * The tunables, each with the reasoning that set it. The sim's own contact thresholds are here
 * rather than in sim/physics.h because what the effect layer reads is the hull damage the sim
 * charged, and that is the speed that caused it.
 */
namespace fx {

/** physics.cpp: damage starts at a 6 m/s closing speed and climbs 0.55 hull per m/s past it. Below
 *  the floor the hull feels nothing, so there is no impact for this layer to see. */
inline constexpr Real CONTACT_FLOOR = 6.0;
inline constexpr Real HULL_PER_SPEED = 0.55;

/** Seconds of sim clock. A spark is a flash; a fracture lingers about as long as its drift. */
inline constexpr Real IMPACT_SECONDS = 0.28;
inline constexpr Real BLAST_SECONDS = 0.80;
inline constexpr Real TRAIL_SECONDS = 0.80;
inline constexpr Real DOCK_SECONDS = 0.60;

/** Sparks: two at the sim's floor and one more every 4 m/s, up to the ceiling of sixteen from
 *  62 m/s (the 42-damage cap implies 82). Half-extents are metres, launch speeds m/s, at the
 *  world's own scale. */
inline constexpr int SPARK_MIN = 2;
/** Brightness is full here: 40 m/s of closing speed, the top of what a stock hull survives well. */
inline constexpr Real SPARK_FULL_SPEED = 40.0;

inline constexpr int SPARK_MAX = 16;
inline constexpr Real SPARK_PER_SPEED = 4.0;
inline constexpr Real SPARK_SIZE_MIN = 1.2;
inline constexpr Real SPARK_SIZE_MAX = 2.2;
inline constexpr Real SPARK_LAUNCH_MIN = 5.0;
inline constexpr Real SPARK_LAUNCH_MAX = 60.0;
inline constexpr Real SPARK_FAN = 0.9;  // radians either side of the contact normal

/** The blast: a puff sized off the rock, and a trail behind each piece it left. */
inline constexpr int DUST_COUNT = 12;
inline constexpr int TRAIL_CUBES = 4;
inline constexpr Real DUST_SIZE = 0.18;   // of the rock's radius, per grain
inline constexpr Real DUST_SPEED = 0.55;  // of the rock's radius, per second
inline constexpr Real DUST_STRENGTH = 0.34;
inline constexpr Real TRAIL_STRENGTH = 0.5;
/** A break is never authored below this size of event: the field's own rocks start at 8 m, and eight
 *  metres of grit at flight zoom is a few pixels. The puff scales up from here, not down from a
 *  number, so a pebble and a boulder both read as breaks. */
inline constexpr Real DUST_MIN_RADIUS = 24.0;
/** Grains are born on a ring, not in a point, and the ring is wide enough to clear the body they
 *  came out of: the frame a fracture happens on is the frame a capture takes, and a cloud born
 *  inside the body it broke is mostly hidden by it. */
inline constexpr Real DUST_BIRTH_RING = 0.4;

/** The dock pulse: one ring, twelve cubes, opening from a 5 m radius at 16 m/s. */
inline constexpr int PULSE_COUNT = 12;
inline constexpr Real PULSE_RADIUS = 5.0;
inline constexpr Real PULSE_SPEED = 16.0;
inline constexpr Real PULSE_SIZE = 1.8;

/** 96 slots hold a 16-spark burst over a 27-particle blast, twice inside one 0.8 s life. */
inline constexpr int POOL_SLOTS = 96;
inline constexpr int POOL_MAX = 256;

/** Bodies are identified by id, handed out in creation order, so 4096 flags are the set of
 *  everything this module has already seen leave the field. Field rocks are far below the bound;
 *  a fragment past it breaks silently rather than dragging a container in here. */
inline constexpr std::size_t SEEN_MAX = 4096;

/**
 * The effect pass carries the flame's vertex profile, which fades alpha along the raw vertex y:
 * pow(1 - t, 1.65) with t = (y + 18) / 36. The star cube spans y in [-0.5, 0.5], where that curve
 * is flat to three places - 0.304 at the bottom, 0.333 at the top, 0.319 mean - so an additive
 * cube shows about a third of the colour the instance carries. The gain puts a full-strength
 * effect back onto its token value, and lets an overlap blow out to white the way a spark should.
 * ponytail: the exact fix is a third additive pipeline without the flame fade, and that pipeline
 * lives in render/renderer.cpp, not here.
 */
inline constexpr float EFFECT_GAIN = 3.14f;

}  // namespace fx

/**
 * One particle. Everything about it - where, how big, how bright - is a pure function of (born,
 * now), so pausing the sim freezes a burst mid-air and a clock reset retires it instead of
 * leaving it stranded at a negative age.
 */
struct Effect {
    EffectKind kind = EffectKind::Spark;
    /** World metres, in the zone frame, already offset for the kind's own shape. */
    Vec2 origin{};
    /** Unit launch direction. */
    Vec2 dir{1.0, 0.0};
    /** Along `dir`, metres per second. */
    Real speed = 0;
    /** Half-extent at birth, metres. */
    Real size = 0;
    /** Sim clock at emission, seconds. */
    Real born = 0;
    /** Sim seconds of life. */
    Real duration = 0;
    /** 0..1 at birth, before the lifetime curve. */
    Real strength = 0;
    /** Tumble rate, radians per second. */
    Real spin = 0;
    /** Tumble at birth, radians. */
    Real phase = 0;
};

/** Sim seconds since emission; negative once the clock has been reset past it. */
Real effect_age(const Effect &effect, Real now);

/** Normalised age: 0 at birth, 1 at the end of life, clamped. */
Real effect_life(const Effect &effect, Real now);

/**
 * The lifetime curve: 1 at birth, 0 at the end of life, (1 - s)^2 between, so a burst is bright for
 * its first third and dies quietly instead of draining linearly. Exactly zero at s >= 1 is what
 * makes "invisible at t = duration" the same statement as "not live".
 */
Real effect_fade(const Effect &effect, Real now);

/** The payload strength on the lifetime curve: what the particle's brightness is scaled by. */
Real effect_intensity(const Effect &effect, Real now);

/** Birth position plus the particle's own velocity times its own age. */
Vec2 effect_position(const Effect &effect, Real now);

/** Half-extent now: a spark shrinks, a grain of dust opens, a trail shortens to nothing. */
Real effect_size(const Effect &effect, Real now);

/** Tumble angle now, radians about z. */
Real effect_angle(const Effect &effect, Real now);

/** The token hue for the kind at the current brightness: additive, so this is the whole fade. */
glm::vec3 effect_color(const Effect &effect, Real now);

/** Deterministic 0..1 from an index: a burst varies without an RNG, so a capture repeats. */
Real hash01(std::uint32_t n);

/** Particles a contact of `speed` throws. Monotone in speed: 2 at the floor, 10 at 40 m/s, and all
 *  16 of the ceiling from 62 m/s up. */
int spark_count(Real speed);

/** A contact's brightness: 0 at the sim's floor, 1 at 40 m/s and above. Monotone in speed. */
Real impact_strength(Real speed);

/**
 * The pool. Slots are reused in the order they were written, so the oldest particles are the ones
 * recycled when it is full: the live count is capped at `capacity()`, the storage never grows and
 * a burst never allocates. `capacity() == 0` is the only case in which a spawn is refused.
 */
struct Effects {
    explicit Effects(int capacity = fx::POOL_SLOTS);

    /**
     * Adds one particle. A free slot first; when every slot is live, the oldest is recycled, which
     * is what keeps a 100-burst frame inside a 16-slot pool.
     */
    bool spawn(const Effect &effect);

    /**
     * Reads this frame's edges out of the world and emits what they imply: sparks for a fresh
     * contact, dust and trails for a fracture, one pulse for a capture. Called every frame with the
     * world the sim just stepped; the first call only records, so a run cannot open on a phantom.
     */
    void update(const World &world);

    /** Slots holding a particle that is still visible at `now`. */
    int live(Real now) const;

    int capacity() const { return capacity_; }
    /** Slots in use: 0..capacity(). Never a particle count, and never above capacity(). */
    int size() const { return count_; }
    const Effect &at(int slot) const { return slots_[static_cast<std::size_t>(slot)]; }

    /** Drops every particle and forgets last frame's edges: the next update only records again. */
    void clear();

private:
    std::array<Effect, fx::POOL_MAX> slots_{};
    /** One bit per body id already seen retired: the fracture edge without a per-frame container. */
    std::array<bool, fx::SEEN_MAX> retired_seen_{};
    int capacity_ = fx::POOL_SLOTS;
    int count_ = 0;
    int next_ = 0;
    // Last frame's world, so an edge can be read from a difference. The sim is the only source:
    // nothing here is a clock of its own.
    Vec2 last_velocity_{};
    Real last_contact_timer_ = 1.0;
    Real last_hull_ = 0.0;
    double last_clock_ = 0.0;
    std::size_t last_fragments_ = 0;
    bool last_docked_ = false;
    bool primed_ = false;
};

/** Emits one burst; returns the particles it added. `born` is the sim clock at emission. */
int emit_impact(Effects &pool, const Vec2 &point, const Vec2 &normal, Real speed, Real born);
int emit_blast(Effects &pool, const Vec2 &centre, Real rock_radius, Real born);
int emit_trail(Effects &pool, const Vec2 &from, const Vec2 &velocity, Real radius, Real born);
int emit_dock_pulse(Effects &pool, const Vec2 &centre, Real born);

/** A piece of wreckage from a detached module (PLAN-08 §11). */
struct DebrisBody {
    std::string part;
    Vec2 pos{0.0, 0.0};
    Vec2 vel{0.0, 0.0};
    Real angle = 0.0;
    Real angular_vel = 0.0;
    Real bounds_radius = 2.5;
    bool retired = false;
};

struct WreckageResult {
    std::vector<DebrisBody> debris;
    int particles = 0;
};

/** Module wreckage separation on ship destruction (PLAN-08 §11). */
WreckageResult emit_wreckage(Effects &pool, const ShipDesign &design, const PartTable &parts,
                             const Vec2 &ship_pos, const Vec2 &ship_vel, Real ship_angle,
                             Real ship_omega, Real blast_energy, Real born,
                             Real station_bounds_radius = 0.0);

/**
 * The process's one pool. The game runs one world in one window, and the pool is keyed to the sim
 * clock, so a fresh run (clock back to zero) retires everything still in it. It lives here rather
 * than on App only because the two callers - the frame and the scene builder - are separate
 * translation units.
 * ponytail: the upgrade path is a member on App the day a second world is drawn at once.
 */
Effects &world_effects();

/** Empties the process's pool. `App::reset_run` calls it, so a new run starts with clean glass. */
void clear_effects();

}  // namespace opra
