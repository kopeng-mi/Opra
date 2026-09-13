// Input state and the mapping from keys to a flight command.
#pragma once

#include <SDL3/SDL.h>
#include <glm/glm.hpp>

#include "sim/physics.h"

namespace opra {

struct Input {
    bool keys[SDL_SCANCODE_COUNT] = {};
    bool previous[SDL_SCANCODE_COUNT] = {};
    bool left = false, previous_left = false;
    bool right = false, previous_right = false;
    glm::vec2 pointer{0.0f, 0.0f};
    /** Movement since the last frame, for the drag-style controls (the camera's pitch, a turntable). */
    glm::vec2 pointer_delta{0.0f, 0.0f};
    bool pointer_valid = false;
    float wheel = 0.0f;

    /** Call once per frame, before polling events, so edge detection works. */
    void begin_frame();

    bool held(SDL_Scancode key) const { return keys[key]; }
    bool pressed(SDL_Scancode key) const { return keys[key] && !previous[key]; }
    bool left_pressed() const { return left && !previous_left; }
    bool left_released() const { return !left && previous_left; }
    bool right_pressed() const { return right && !previous_right; }
    bool right_released() const { return !right && previous_right; }
};

/** Applies one SDL event to the input state. Returns false when the window should close. */
bool apply_event(const SDL_Event &event, Input &input);

FlightInput flight_input_from(const Input &input);

}  // namespace opra
