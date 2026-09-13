// Immediate-mode UI: layout, hit-testing and the standard hot/active state machine, drawing only
// through ui/draw.h in the HUD's mark language. The context owns no GPU state: it fills a UIBatch.
#pragma once

#include <cstdint>

#include <glm/glm.hpp>

#include "ui/draw.h"
#include "ui/tokens.h"

namespace opra::ui {

struct Rect {
    float x = 0.0f, y = 0.0f, w = 0.0f, h = 0.0f;

    bool contains(const glm::vec2 &point) const {
        return point.x >= x && point.x <= x + w && point.y >= y && point.y <= y + h;
    }
    glm::vec2 centre() const { return {x + w * 0.5f, y + h * 0.5f}; }
};

struct Pointer {
    glm::vec2 at{0.0f};
    bool down = false;
    bool pressed = false;
    bool released = false;
    bool right_down = false;
    bool right_pressed = false;
    float wheel = 0.0f;
    bool valid = false;
};

/** Keyboard navigation for this frame: Tab and arrows move focus, Enter fires. */
struct Nav {
    bool next = false;
    bool previous = false;
    bool activate = false;
    bool decrease = false;
    bool increase = false;
};

/** FNV-1a of the id string, salted with the index for widgets built in a loop. */
uint32_t hash_id(const char *text, int index = 0);

class Context {
public:
    void begin(UIBatch &batch, const glm::vec2 &screen, const Pointer &pointer, const Nav &nav,
               double now);
    double time() const { return now_; }
    const Pointer &pointer() const { return pointer_; }
    /** Applies pending keyboard navigation and clears the transient state. */
    void end();

    // ---- layout: a stack of rects, each cut consumes from the current one
    void push(const Rect &rect);
    void pop();
    Rect area() const { return stack_.empty() ? Rect{} : stack_.back(); }
    Rect cut_top(float height);
    Rect cut_bottom(float height);
    Rect cut_left(float width);
    Rect cut_right(float width);
    Rect inset(const Rect &rect, float by) const;
    /** Splits a rect into `count` columns with `gap` between them. */
    Rect column(const Rect &rect, int index, int count, float gap) const;

    // ---- widgets: they draw and report interaction
    bool button(const char *id, const Rect &at, const char *label, int index = 0);
    /**
     * A list row: the same hit test and focus behaviour as `button`, with no rule under it. Five
     * rows in a list are already five rows; a rule under each is decoration repeating, not structure
     * (R-6). The rule belongs under the block, not under the row.
     */
    bool row(const char *id, const Rect &at, const char *label, int index = 0,
             const glm::vec4 &ink = tokens::ETCH);
    /** A filled surface: chart plates and scrims, never the flight glass (tokens.h: two materials). */
    void panel(const Rect &at, const glm::vec4 &color);
    bool toggle(const char *id, const Rect &at, const char *label, bool &value, int index = 0);
    bool slider(const char *id, const Rect &at, const char *label, float &value, float lo, float hi,
                int index = 0);
    /** A one-of-N row: clicking it selects `option`. */
    bool radio(const char *id, const Rect &at, const char *label, int &selected, int option,
               int index = 0);
    /** A scrolling-free list of rows; returns true when the selection changed. */
    bool list(const char *id, const Rect &at, const char *const *items, int count, int &selected,
              int index = 0);

    // ---- plain marks
    void label(const Rect &at, const char *text, float px, TextAlign align, const glm::vec4 &color,
               TextFace face = TextFace::Readout);
    void rule(const Rect &at, const glm::vec4 &color);
    void section(const Rect &at, const char *text);
    /** True when this id currently has keyboard focus. */
    bool focused(const char *id, int index = 0) const { return focus_ == hash_id(id, index); }

    /** The batch being filled; screens that draw their own marks use it directly. */
    UIBatch *batch() { return batch_; }

    uint32_t hot() const { return hot_; }
    uint32_t active() const { return active_; }
    bool keyboard_in_use() const { return keyboard_; }

private:
    /** The state machine: hover, press, release-inside, plus Enter on the focused widget. */
    bool interact(uint32_t id, const Rect &at);
    bool lit(uint32_t id, const Rect &at) const;

    UIBatch *batch_ = nullptr;
    glm::vec2 screen_{0.0f};
    Pointer pointer_;
    Nav nav_;
    std::vector<Rect> stack_;
    std::vector<uint32_t> order_;
    uint32_t hot_ = 0;
    uint32_t active_ = 0;
    uint32_t focus_ = 0;
    bool keyboard_ = false;
    /** Set when the pointer presses this frame, so a held button does not keep re-latching. */
    bool pressed_this_frame_ = false;
    double now_ = 0.0;
    uint32_t last_fired_ = 0;
    double last_fired_at_ = -10.0;
};

}  // namespace opra::ui
