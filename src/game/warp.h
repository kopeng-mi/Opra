// The warp rail: a stepped multiplier on simulation time, and the rules that drop it. Above 10x the
// ship is advanced on its conic instead of integrated (E7), which is exact at any step - so the
// only thing that can go wrong with warp is warp itself, and that is what this file decides.
#pragma once

namespace opra {

/** The rail, in order. A warp step is not a bigger dt: the sim still steps at its fixed rate. */
inline constexpr double WARP_RATES[] = {1.0, 10.0, 100.0, 1.0e3, 1.0e4, 1.0e5};
inline constexpr int WARP_RATE_COUNT = 6;
/** Above this the ship rides its conic: thrust is refused rather than smeared over a huge step. */
inline constexpr double WARP_RAIL_ABOVE = 10.0;
/** Ceiling on one frame's railed advance, so a stall cannot teleport the ship a month ahead. */
inline constexpr double WARP_MAX_ADVANCE = 86400.0;

struct Warp {
    /** Why the warp dropped itself, for the HUD to say. `None` means it is still up. */
    enum class Drop { None, Thrust, Contact, Sphere, Air, Manual, Zoom };

    int step = 0;
    Drop drop = Drop::None;

    double rate() const;
    /** True when the ship must be advanced analytically: no thrust, no contact, warp above 10x. */
    bool railed() const;

    /** The player's key: one step up or down, and the drop reason is cleared. */
    void request(int delta);

    /**
     * The automatic part, once per frame. Returns true when this call dropped the warp. `in_air`
     * is true while the ship is inside a body's atmosphere: drag breaks the rails, because a ship
     * being decelerated by air is not on a conic at all (plan 3.3).
     */
    bool update(bool thrusting, bool in_contact, bool switched_sphere, bool in_air);

    /** Drops to 1x because the player asked (a manual toggle, a planner edit). */
    bool drop_to_real_time(Drop reason);
};

}  // namespace opra
