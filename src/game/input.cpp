#include "game/input.h"

#include "game/bindings.h"

#include <cstring>

namespace opra {

void Input::begin_frame() {
    std::memcpy(previous, keys, sizeof keys);
    previous_left = left;
    previous_right = right;
    previous_middle = middle;
    pointer_delta = glm::vec2(0.0f, 0.0f);
    wheel = 0.0f;
}

bool apply_event(const SDL_Event &event, Input &input) {
    switch (event.type) {
        case SDL_EVENT_QUIT:
            return false;
        case SDL_EVENT_KEY_DOWN:
            if (event.key.scancode < SDL_SCANCODE_COUNT) input.keys[event.key.scancode] = true;
            break;
        case SDL_EVENT_KEY_UP:
            if (event.key.scancode < SDL_SCANCODE_COUNT) input.keys[event.key.scancode] = false;
            break;
        case SDL_EVENT_MOUSE_MOTION:
            input.pointer = glm::vec2(event.motion.x, event.motion.y);
            input.pointer_delta += glm::vec2(event.motion.xrel, event.motion.yrel);
            input.pointer_valid = true;
            break;
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
            if (event.button.button == SDL_BUTTON_LEFT) input.left = true;
            if (event.button.button == SDL_BUTTON_RIGHT) input.right = true;
            if (event.button.button == SDL_BUTTON_MIDDLE) input.middle = true;
            input.pointer = glm::vec2(event.button.x, event.button.y);
            input.pointer_valid = true;
            break;
        case SDL_EVENT_MOUSE_BUTTON_UP:
            if (event.button.button == SDL_BUTTON_LEFT) input.left = false;
            if (event.button.button == SDL_BUTTON_RIGHT) input.right = false;
            if (event.button.button == SDL_BUTTON_MIDDLE) input.middle = false;
            break;
        case SDL_EVENT_MOUSE_WHEEL:
            input.wheel += event.wheel.y;
            break;
        case SDL_EVENT_WINDOW_FOCUS_LOST:
            std::memset(input.keys, 0, sizeof input.keys);
            input.left = false;
            input.right = false;
            input.middle = false;
            break;
        default:
            break;
    }
    return true;
}

FlightInput flight_input_from(const Input &input) {
    // The keys are data (game/bindings.h); this maps actions onto the sim's five-field request.
    // The turn axis is negated here: the original ran on a y-down canvas where a positive angle
    // read as clockwise, this one runs on a y-up world and camera where it reads anticlockwise, so
    // the same numeric convention would steer the ship opposite to the key. Inverting the input
    // keeps the simulation - and its golden determinism values - untouched.
    FlightInput flight;
    flight.thrust = (held(input, Action::DriveForward) ? 1.0 : 0.0) +
                    (held(input, Action::DriveReverse) ? -1.0 : 0.0);
    flight.turn = (held(input, Action::TurnLeft) ? 1.0 : 0.0) +
                  (held(input, Action::TurnRight) ? -1.0 : 0.0);
    flight.strafe = (held(input, Action::StrafeRight) ? 1.0 : 0.0) +
                    (held(input, Action::StrafeLeft) ? -1.0 : 0.0);
    flight.brake = held(input, Action::KillVelocity);
    flight.boost = held(input, Action::Boost);
    return flight;
}

}  // namespace opra
