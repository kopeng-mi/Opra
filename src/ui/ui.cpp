#include "ui/ui.h"

#include <algorithm>
#include <cstdio>
#include <vector>

#include "hud/hud.h"

namespace opra::ui {

uint32_t hash_id(const char *text, int index) {
    uint32_t hash = 2166136261u;
    for (const char *at = text; at && *at; ++at) {
        hash ^= static_cast<unsigned char>(*at);
        hash *= 16777619u;
    }
    hash ^= static_cast<uint32_t>(index) + 0x9e3779b9u;
    hash *= 16777619u;
    return hash ? hash : 1u;
}

void Context::begin(UIBatch &batch, const glm::vec2 &screen, const Pointer &pointer,
                    const Nav &nav) {
    batch_ = &batch;
    screen_ = screen;
    pointer_ = pointer;
    nav_ = nav;
    stack_.clear();
    order_.clear();
    hot_ = 0;
    // The pointer is up and this is not the release frame: nothing can still be held, so no
    // widget may stay active - including one that stopped being submitted while it was.
    if (!pointer.down && !pointer.released) active_ = 0;
    pressed_this_frame_ = pointer.pressed;
    if (pointer.pressed || pointer.wheel != 0.0f) keyboard_ = false;
    if (nav.next || nav.previous || nav.activate || nav.decrease || nav.increase) keyboard_ = true;
}

void Context::end() {
    if (order_.empty()) {
        focus_ = 0;
        stack_.clear();
        return;
    }
    // Keyboard focus walks the order widgets registered this frame, so Tab follows the screen.
    if (nav_.next || nav_.previous) {
        const int count = static_cast<int>(order_.size());
        int at = 0;
        for (int i = 0; i < count; ++i) {
            if (order_[static_cast<size_t>(i)] == focus_) at = i;
        }
        at = (at + (nav_.next ? 1 : count - 1)) % count;
        focus_ = order_[static_cast<size_t>(at)];
    }
    if (focus_ && std::find(order_.begin(), order_.end(), focus_) == order_.end()) focus_ = 0;
    if (!focus_) focus_ = order_.front();

    // The hot id belongs to the widget under the cursor; nothing claimed it otherwise.
    stack_.clear();
}

void Context::push(const Rect &rect) { stack_.push_back(rect); }

void Context::pop() {
    if (!stack_.empty()) stack_.pop_back();
}

Rect Context::cut_top(float height) {
    if (stack_.empty()) return Rect{};
    Rect &top = stack_.back();
    const Rect out{top.x, top.y, top.w, std::min(height, top.h)};
    top.y += out.h;
    top.h -= out.h;
    return out;
}

Rect Context::cut_bottom(float height) {
    if (stack_.empty()) return Rect{};
    Rect &top = stack_.back();
    const float taken = std::min(height, top.h);
    const Rect out{top.x, top.y + top.h - taken, top.w, taken};
    top.h -= taken;
    return out;
}

Rect Context::cut_left(float width) {
    if (stack_.empty()) return Rect{};
    Rect &top = stack_.back();
    const float taken = std::min(width, top.w);
    const Rect out{top.x, top.y, taken, top.h};
    top.x += taken;
    top.w -= taken;
    return out;
}

Rect Context::cut_right(float width) {
    if (stack_.empty()) return Rect{};
    Rect &top = stack_.back();
    const float taken = std::min(width, top.w);
    const Rect out{top.x + top.w - taken, top.y, taken, top.h};
    top.w -= taken;
    return out;
}

Rect Context::inset(const Rect &rect, float by) const {
    return Rect{rect.x + by, rect.y + by, rect.w - by * 2.0f, rect.h - by * 2.0f};
}

Rect Context::column(const Rect &rect, int index, int count, float gap) const {
    const float width = (rect.w - gap * static_cast<float>(count - 1)) / static_cast<float>(count);
    return Rect{rect.x + static_cast<float>(index) * (width + gap), rect.y, width, rect.h};
}

bool Context::lit(uint32_t id, const Rect &at) const {
    return (pointer_.valid && at.contains(pointer_.at)) || active_ == id || focus_ == id;
}

bool Context::interact(uint32_t id, const Rect &at) {
    const bool inside = pointer_.valid && at.contains(pointer_.at);
    if (inside) hot_ = id;
    if (inside && pressed_this_frame_) active_ = id;
    if (active_ == id && pointer_.released) {
        const bool fired = inside;
        active_ = 0;
        if (fired) {
            focus_ = id;
            return true;
        }
    }
    if (focus_ == id && nav_.activate) return true;
    return false;
}

void Context::label(const Rect &at, const char *text, float px, TextAlign align,
                    const glm::vec4 &color, TextFace face) {
    if (!batch_) return;
    const float y = at.y + (at.h - px) * 0.5f;
    push_text(*batch_, text, px, {at.x, y}, align, color, face);
}

void Context::rule(const Rect &at, const glm::vec4 &color) {
    if (!batch_) return;
    push_rect(*batch_, {at.x, at.y + at.h - 1.0f}, {at.w, 1.0f}, color);
}

void Context::section(const Rect &at, const char *text) {
    label(at, text, 15.0f, TextAlign::Left, ui::tokens::ETCH_DIM, TextFace::Label);
    rule({at.x, at.y + at.h - 6.0f, at.w, at.h}, with_alpha(ui::tokens::ETCH_DIM, 0.35f));
}

bool Context::button(const char *id, const Rect &at, const char *label_text, int index) {
    const uint32_t widget = hash_id(id, index);
    const bool fired = interact(widget, at);
    const bool bright = lit(widget, at);
    const glm::vec4 color = bright ? ui::tokens::ETCH : ui::tokens::ETCH_DIM;
    if (batch_) {
        if (focused(id, index) && keyboard_) {
            push_rect(*batch_, {at.x - 14.0f, at.y + 4.0f}, {3.0f, at.h - 8.0f}, ui::tokens::NAV);
        }
        push_text(*batch_, label_text, 19.0f, {at.x, at.y + (at.h - 19.0f) * 0.5f}, TextAlign::Left,
                  color);
        rule(at, bright ? ui::tokens::ETCH : with_alpha(ui::tokens::ETCH_DIM, 0.5f));
    }
    order_.push_back(widget);
    return fired;
}

bool Context::row(const char *id, const Rect &at, const char *label_text, int index,
                  const glm::vec4 &ink) {
    const uint32_t widget = hash_id(id, index);
    const bool fired = interact(widget, at);
    const bool bright = lit(widget, at);
    const glm::vec4 color = bright ? ink : with_alpha(ink, tokens::DORMANT);
    if (batch_) {
        if (focused(id, index) && keyboard_) {
            push_rect(*batch_, {at.x - 14.0f, at.y + 4.0f}, {3.0f, at.h - 8.0f}, ui::tokens::NAV);
        }
        push_text(*batch_, label_text, 19.0f, {at.x, at.y + (at.h - 19.0f) * 0.5f}, TextAlign::Left,
                  color);
    }
    order_.push_back(widget);
    return fired;
}

void Context::panel(const Rect &at, const glm::vec4 &color) {
    if (batch_) push_rect(*batch_, {at.x, at.y}, {at.w, at.h}, color);
}

bool Context::toggle(const char *id, const Rect &at, const char *label_text, bool &value,
                     int index) {
    const uint32_t widget = hash_id(id, index);
    const bool fired = interact(widget, at);
    if (fired) value = !value;
    const bool bright = lit(widget, at);
    if (batch_) {
        if (focused(id, index) && keyboard_) {
            push_rect(*batch_, {at.x - 14.0f, at.y + 4.0f}, {3.0f, at.h - 8.0f}, ui::tokens::NAV);
        }
        const glm::vec4 color = bright ? ui::tokens::ETCH : ui::tokens::ETCH_DIM;
        push_text(*batch_, label_text, 17.0f, {at.x, at.y + (at.h - 17.0f) * 0.5f}, TextAlign::Left,
                  color);
        // The mark: a box that fills when set, never a switch graphic.
        const float box = 12.0f;
        const Rect mark{at.x + at.w - 90.0f, at.y + (at.h - box) * 0.5f, box, box};
        push_rect(*batch_, {mark.x, mark.y}, {mark.w, 1.0f}, with_alpha(color, 0.6f));
        push_rect(*batch_, {mark.x, mark.y + mark.h}, {mark.w, 1.0f}, with_alpha(color, 0.6f));
        push_rect(*batch_, {mark.x, mark.y}, {1.0f, mark.h}, with_alpha(color, 0.6f));
        push_rect(*batch_, {mark.x + mark.w, mark.y}, {1.0f, mark.h}, with_alpha(color, 0.6f));
        if (value) push_rect(*batch_, {mark.x + 3.0f, mark.y + 3.0f}, {mark.w - 6.0f, mark.h - 6.0f}, color);
        push_text(*batch_, value ? "on" : "off", 13.0f,
                  {mark.x + mark.w + 12.0f, at.y + (at.h - 13.0f) * 0.5f}, TextAlign::Left,
                  with_alpha(color, 0.9f), TextFace::Label);
    }
    order_.push_back(widget);
    return fired;
}

bool Context::slider(const char *id, const Rect &at, const char *label_text, float &value, float lo,
                     float hi, int index) {
    const uint32_t widget = hash_id(id, index);
    bool changed = false;
    const bool inside = pointer_.valid && at.contains(pointer_.at);
    if (inside) hot_ = widget;
    if (inside && pressed_this_frame_) active_ = widget;
    if (active_ == widget) {
        if (pointer_.down) {
            const float t = std::clamp((pointer_.at.x - at.x) / std::max(at.w, 1.0f), 0.0f, 1.0f);
            const float next = lo + (hi - lo) * t;
            if (next != value) {
                value = next;
                changed = true;
            }
        }
        if (pointer_.released) {
            if (inside) focus_ = widget;
            active_ = 0;
        }
    }
    if (inside && pointer_.wheel != 0.0f) {
        value = std::clamp(value + (hi - lo) * 0.05f * pointer_.wheel, lo, hi);
        changed = true;
        focus_ = widget;
    }
    if (focus_ == widget && (nav_.decrease || nav_.increase)) {
        value = std::clamp(value + (nav_.increase ? 1.0f : -1.0f) * (hi - lo) * 0.05f, lo, hi);
        changed = true;
    }

    const bool bright = lit(widget, at);
    const glm::vec4 color = bright ? ui::tokens::ETCH : ui::tokens::ETCH_DIM;
    if (batch_) {
        if (focused(id, index) && keyboard_) {
            push_rect(*batch_, {at.x - 14.0f, at.y + 4.0f}, {3.0f, at.h - 8.0f}, ui::tokens::NAV);
        }
        push_text(*batch_, label_text, 17.0f, {at.x, at.y + (at.h - 17.0f) * 0.5f}, TextAlign::Left,
                  color);
        const float track_w = std::max(at.w * 0.45f, 40.0f);
        const float track_x = at.x + at.w - track_w;
        const float track_y = at.y + at.h * 0.5f;
        push_rect(*batch_, {track_x, track_y}, {track_w, 1.0f}, with_alpha(ui::tokens::ETCH_DIM, 0.5f));
        const float t = std::clamp((value - lo) / (hi - lo), 0.0f, 1.0f);
        push_rect(*batch_, {track_x, track_y}, {track_w * t, 1.0f}, color);
        push_rect(*batch_, {track_x + track_w * t - 1.0f, track_y - 5.0f}, {2.0f, 11.0f}, color);
        char text[32];
        std::snprintf(text, sizeof text, "%.2f", static_cast<double>(value));
        push_text(*batch_, text, 14.0f, {at.x + at.w, at.y + (at.h - 14.0f) * 0.5f},
                  TextAlign::Right, with_alpha(color, 0.9f), TextFace::Label);
    }
    order_.push_back(widget);
    return changed;
}

bool Context::radio(const char *id, const Rect &at, const char *label_text, int &selected,
                    int option, int index) {
    const uint32_t widget = hash_id(id, index);
    const bool fired = interact(widget, at);
    if (fired && selected != option) {
        selected = option;
        order_.push_back(widget);
        return true;
    }
    const bool bright = lit(widget, at) || selected == option;
    const glm::vec4 color = bright ? ui::tokens::ETCH : ui::tokens::ETCH_DIM;
    if (batch_) {
        if (focused(id, index) && keyboard_) {
            push_rect(*batch_, {at.x - 14.0f, at.y + 4.0f}, {3.0f, at.h - 8.0f}, ui::tokens::NAV);
        }
        push_text(*batch_, label_text, 17.0f, {at.x, at.y + (at.h - 17.0f) * 0.5f}, TextAlign::Left,
                  color);
        const float marker = at.x + at.w - 20.0f;
        const float mid = at.y + at.h * 0.5f;
        if (selected == option) {
            push_disc(*batch_, {marker, mid}, 3.5f, ui::tokens::NAV);
        } else {
            push_rect(*batch_, {marker - 3.0f, mid}, {7.0f, 1.0f}, with_alpha(color, 0.5f));
        }
    }
    order_.push_back(widget);
    return false;
}

bool Context::list(const char *id, const Rect &at, const char *const *items, int count,
                   int &selected, int index) {
    bool changed = false;
    const uint32_t base = hash_id(id, index);
    if (count <= 0) return false;
    const float row_h = std::min(26.0f, at.h / static_cast<float>(count));

    // Keyboard: the list as a whole takes focus, then the arrows walk its rows.
    const bool inside = pointer_.valid && at.contains(pointer_.at);
    if (inside) hot_ = base;
    if (focus_ == base) {
        if (nav_.increase && selected + 1 < count) {
            selected += 1;
            changed = true;
        }
        if (nav_.decrease && selected > 0) {
            selected -= 1;
            changed = true;
        }
    }

    for (int i = 0; i < count; ++i) {
        const Rect row{at.x, at.y + static_cast<float>(i) * row_h, at.w, row_h};
        const uint32_t widget = hash_id(id, i);
        if (pointer_.valid && row.contains(pointer_.at) && pressed_this_frame_) {
            focus_ = base;
            active_ = widget;
        }
        if (active_ == widget && pointer_.released) {
            active_ = 0;
            if (row.contains(pointer_.at) && selected != i) {
                selected = i;
                changed = true;
            }
        }
        const bool hot_row = pointer_.valid && row.contains(pointer_.at);
        const bool chosen = selected == i;
        const glm::vec4 color = (hot_row || chosen) ? ui::tokens::ETCH : ui::tokens::ETCH_DIM;
        if (batch_) {
            push_text(*batch_, items[i], chosen ? 17.0f : 16.0f,
                      {row.x + 8.0f, row.y + (row.h - 17.0f) * 0.5f}, TextAlign::Left, color,
                      chosen ? TextFace::Readout : TextFace::Label);
            if (chosen) push_rect(*batch_, {row.x, row.y + row.h - 2.0f}, {4.0f, 2.0f}, ui::tokens::NAV);
        }
    }
    if (batch_) push_rect(*batch_, {at.x, at.y}, {1.0f, row_h * static_cast<float>(count)},
                          with_alpha(ui::tokens::ETCH_DIM, 0.4f));
    order_.push_back(base);
    return changed;
}

}  // namespace opra::ui
