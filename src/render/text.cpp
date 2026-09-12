#include "render/text.h"

#include <algorithm>
#include <cmath>

#include "core/log.h"

namespace opra {
namespace {

/** Live TTF_Text objects across every face and size. A frame's numbers reuse their objects; this
 *  is the ceiling that keeps a long session from pinning every string it has ever drawn. */
constexpr int kMaxTexts = 256;

int face_index(TextFace face) {
    switch (face) {
        case TextFace::Display: return 0;
        case TextFace::Readout: return 1;
        case TextFace::Label: return 2;
    }
    return 1;
}

/** Sizes are pixels in this UI, never fractions; the cache key has to be an integer. */
int px_of(float px) {
    const int rounded = static_cast<int>(std::lround(px));
    return rounded < 1 ? 1 : rounded;
}

bool is_digit(uint32_t codepoint) { return codepoint >= '0' && codepoint <= '9'; }

/** Decodes one UTF-8 codepoint; advances `i`. Bytes it cannot decode become '?'. */
uint32_t next_codepoint(const std::string &text, size_t &i) {
    const unsigned char first = static_cast<unsigned char>(text[i]);
    if (first < 0x80) {
        ++i;
        return first;
    }
    int extra = 0;
    uint32_t codepoint = 0;
    if ((first & 0xE0) == 0xC0) {
        extra = 1;
        codepoint = first & 0x1Fu;
    } else if ((first & 0xF0) == 0xE0) {
        extra = 2;
        codepoint = first & 0x0Fu;
    } else if ((first & 0xF8) == 0xF0) {
        extra = 3;
        codepoint = first & 0x07u;
    } else {
        ++i;
        return '?';
    }
    if (i + static_cast<size_t>(extra) >= text.size()) {
        ++i;
        return '?';
    }
    for (int n = 0; n < extra; ++n) {
        const unsigned char continuation = static_cast<unsigned char>(text[i + 1 + n]);
        if ((continuation & 0xC0) != 0x80) {
            ++i;
            return '?';
        }
        codepoint = (codepoint << 6) | (continuation & 0x3Fu);
    }
    i += static_cast<size_t>(extra) + 1;
    return codepoint;
}

}  // namespace

void TextEngine::set_fonts(std::string display, std::string readout, std::string label) {
    face_paths_[0] = std::move(display);
    face_paths_[1] = std::move(readout);
    face_paths_[2] = std::move(label);
}

void TextEngine::attach(gpu::Device &device) {
    if (engine_) return;
    engine_ = TTF_CreateGPUTextEngine(device.handle);
    if (!engine_) fatal("TTF_CreateGPUTextEngine");
    // The draw data is y-up and build() flips it for screen space, which reverses the winding:
    // ask SDL_ttf for clockwise so the quads reach the frame counter-clockwise, the UI pipeline's
    // front face. That pipeline culls nothing, so either order would draw.
    TTF_SetGPUTextEngineWinding(engine_, TTF_GPU_TEXTENGINE_WINDING_CLOCKWISE);
}

TextEngine::Slot &TextEngine::slot_for(TextFace face, int px) {
    const int index = face_index(face);
    Slot &slot = slots_[index][px];
    if (slot.font) return slot;
    const std::string &path = face_paths_[index];
    slot.font = TTF_OpenFont(path.c_str(), static_cast<float>(px));
    if (!slot.font) {
        SDL_Log("TTF_OpenFont(%s) at %d px: %s", path.c_str(), px, SDL_GetError());
        fatal("TTF_OpenFont");
    }
    slot.ascent = TTF_GetFontAscent(slot.font);
    // Tabular figures the faces do not ship (PLAN-02 s4.3): every digit takes the widest digit's
    // advance, so a readout counting through 199 -> 200 keeps its width. Measured per size, from
    // the same cached glyphs SDL_ttf lays out with, so the grid matches the drawn quads exactly.
    int widest = 0;
    for (uint32_t digit = '0'; digit <= '9'; ++digit) {
        int min_x = 0, max_x = 0, min_y = 0, max_y = 0, advance = 0;
        if (TTF_GetGlyphMetrics(slot.font, digit, &min_x, &max_x, &min_y, &max_y, &advance) &&
            advance > widest) {
            widest = advance;
        }
    }
    slot.digit_advance = widest;
    return slot;
}

TextEngine::Layout TextEngine::lay_out(const Slot &slot, const std::string &text, bool placement) {
    Layout layout;
    layout.exact = true;
    if (placement) {
        layout.placed.reserve(text.size());
        layout.order.reserve(text.size());
    }
    float pen = 0.0f;
    float shift = 0.0f;
    float last_ink_x = 0.0f;
    bool drew = false;
    uint32_t previous = 0;
    for (size_t i = 0; i < text.size();) {
        const uint32_t codepoint = next_codepoint(text, i);
        // A second line restarts both the pen and the ascent line, and this walk is one line.
        if (codepoint == '\n' || codepoint == '\r') layout.exact = false;
        int min_x = 0, max_x = 0, min_y = 0, max_y = 0, advance = 0;
        if (!TTF_GetGlyphMetrics(slot.font, codepoint, &min_x, &max_x, &min_y, &max_y, &advance)) {
            // SDL_ttf still draws its .notdef for this codepoint; the walk no longer describes it.
            layout.exact = false;
        }
        // Kerning, except across a digit: the grid is what keeps the columns in line, and a kerned
        // digit pair would break it as surely as a proportional advance would.
        if (previous && !is_digit(previous) && !is_digit(codepoint)) {
            int kerning = 0;
            if (TTF_GetGlyphKerning(slot.font, previous, codepoint, &kerning)) {
                pen += static_cast<float>(kerning);
            }
        }
        const bool gridded = slot.digit_advance > 0 && is_digit(codepoint);
        if (gridded) layout.digits = true;
        if (placement) {
            const bool draws = max_x > min_x && max_y > min_y;
            const float ink_x = pen + static_cast<float>(min_x);
            // build() recovers the quads' string order by sorting them on ink x, which only holds
            // while the ink rises with the pen; a glyph that hangs left of its own pen breaks it.
            if (draws) {
                if (drew && ink_x <= last_ink_x) layout.exact = false;
                last_ink_x = ink_x;
                drew = true;
            }
            layout.placed.push_back(Placed{shift, draws});
            if (draws) layout.order.push_back(static_cast<uint32_t>(layout.placed.size() - 1));
        }
        if (max_y > layout.ink_top) layout.ink_top = max_y;
        pen += static_cast<float>(gridded ? slot.digit_advance : advance);
        if (gridded) shift += static_cast<float>(slot.digit_advance - advance);
        previous = codepoint;
    }
    layout.width = pen;
    return layout;
}

TTF_Text *TextEngine::text_for(Slot &slot, const std::string &text) {
    for (CachedText &entry : slot.texts) {
        if (entry.text == text) {
            entry.used = frame_;
            return entry.object;
        }
    }
    // A value that changes every frame misses the key every frame; re-setting an object this frame
    // has not asked for costs one layout and no allocation at all, which is the point of the cache.
    CachedText *reusable = nullptr;
    for (CachedText &entry : slot.texts) {
        if (entry.used != frame_ && (!reusable || entry.used < reusable->used)) reusable = &entry;
    }
    if (reusable) {
        if (!TTF_SetTextString(reusable->object, text.c_str(), text.size())) {
            fatal("TTF_SetTextString");
        }
        reusable->text = text;
        reusable->used = frame_;
        return reusable->object;
    }
    if (live_texts_ >= kMaxTexts) evict_text();
    TTF_Text *object = TTF_CreateText(engine_, slot.font, text.c_str(), text.size());
    if (!object) fatal("TTF_CreateText");
    slot.texts.push_back(CachedText{object, text, frame_});
    ++live_texts_;
    return object;
}

void TextEngine::evict_text() {
    Slot *owner = nullptr;
    size_t index = 0;
    for (int face = 0; face < kFaceCount; ++face) {
        for (auto &pair : slots_[face]) {
            Slot &slot = pair.second;
            for (size_t i = 0; i < slot.texts.size(); ++i) {
                if (!owner || slot.texts[i].used < owner->texts[index].used) {
                    owner = &slot;
                    index = i;
                }
            }
        }
    }
    if (!owner) return;
    // An entry this frame has already used is safe to drop: its quads were copied into the frame's
    // buffers the moment it was laid out, and nothing reads the object again.
    TTF_DestroyText(owner->texts[index].object);
    owner->texts.erase(owner->texts.begin() + static_cast<ptrdiff_t>(index));
    --live_texts_;
}

float TextEngine::measure(const TextDraw &draw) {
    if (draw.text.empty()) return 0.0f;
    return lay_out(slot_for(draw.face, px_of(draw.px)), draw.text, false).width;
}

void TextEngine::build(const std::vector<TextDraw> &texts, std::vector<UIVertex> &vertices,
                       std::vector<uint32_t> &indices, std::vector<TextRun> &runs) {
    ++frame_;
    if (texts.empty()) return;
    if (!engine_) fatal("TextEngine::build");
    for (const TextDraw &draw : texts) {
        if (draw.text.empty()) continue;
        Slot &slot = slot_for(draw.face, px_of(draw.px));
        const Layout layout = lay_out(slot, draw.text, true);
        TTF_GPUAtlasDrawSequence *sequence = TTF_GetGPUTextDrawData(text_for(slot, draw.text));
        if (!sequence) continue;  // all whitespace: SDL_ttf hands back nothing to draw
        int quads = 0;
        for (TTF_GPUAtlasDrawSequence *s = sequence; s; s = s->next) quads += s->num_vertices / 4;
        // SDL_ttf drops the quad of a glyph with an empty ink box (a space) and of a clipped one;
        // when its quad count matches the walk, the two sets describe the same glyphs. A string
        // without a digit needs none of this: the grid shifts nothing, so it is drawn exactly
        // where SDL_ttf laid it out.
        const bool gridded = layout.digits && layout.exact &&
                             quads == static_cast<int>(layout.order.size());
        if (gridded) {
            // SDL_ttf sorts a text's draw operations by atlas before handing them back
            // (SDL_gpu_textengine.c, "Sort the operations to batch by texture"), so a sequence's
            // quads are NOT in string order - a 25-character label comes back permuted. The
            // coordinates are untouched and a left-to-right line's ink x rises with the pen, so
            // sorting the quads on x recovers the order the walk describes.
            quad_x_.clear();
            quad_order_.clear();
            for (TTF_GPUAtlasDrawSequence *s = sequence; s; s = s->next) {
                for (int i = 0; i < s->num_vertices; i += 4) {
                    quad_x_.push_back(s->xy[i].x);
                    quad_order_.push_back(static_cast<uint32_t>(quad_order_.size()));
                }
            }
            std::sort(quad_order_.begin(), quad_order_.end(), [this](uint32_t a, uint32_t b) {
                return quad_x_[a] < quad_x_[b];
            });
            quad_shift_.assign(quad_order_.size(), 0.0f);
            for (size_t rank = 0; rank < quad_order_.size(); ++rank) {
                const Placed &placed = layout.placed[layout.order[rank]];
                quad_shift_[quad_order_[rank]] = placed.shift;
            }
        }
        float left = draw.at.x;
        if (draw.align == TextAlign::Center) {
            left -= layout.width * 0.5f;
        } else if (draw.align == TextAlign::Right) {
            left -= layout.width;
        }
        // xy is y-up and its origin is the ascent line, pushed down when the tallest glyph stands
        // above it: the baseline is at `top` plus the ascent.
        const int ystart = std::max(0, layout.ink_top - slot.ascent);
        const float top = draw.at.y - static_cast<float>(ystart);
        int quad = 0;
        for (TTF_GPUAtlasDrawSequence *s = sequence; s; s = s->next) {
            if (!s->atlas_texture || !s->xy || !s->uv || !s->indices) {
                quad += s->num_vertices / 4;  // a quad this frame cannot sample: never appended
                continue;
            }
            const uint32_t first_vertex = static_cast<uint32_t>(vertices.size());
            const uint32_t first_index = static_cast<uint32_t>(indices.size());
            for (int i = 0; i < s->num_vertices; i += 4, ++quad) {
                const float dx = left + (gridded ? quad_shift_[static_cast<size_t>(quad)] : 0.0f);
                for (int v = 0; v < 4; ++v) {
                    UIVertex vertex;
                    vertex.pos = glm::vec2(s->xy[i + v].x + dx, top - s->xy[i + v].y);
                    vertex.uv = glm::vec2(s->uv[i + v].x, s->uv[i + v].y);
                    vertex.color = draw.color;
                    vertices.push_back(vertex);
                }
                const int base = (i / 4) * 6;
                for (int v = 0; v < 6; ++v) {
                    indices.push_back(first_vertex +
                                      static_cast<uint32_t>(s->indices[base + v]));
                }
            }
            if (s->num_indices <= 0) continue;
            // Two sequences of one string share an atlas only when they are adjacent; a run that
            // folded them together across a texture switch would draw the wrong glyphs.
            TextRun *last = runs.empty() ? nullptr : &runs.back();
            if (last && last->texture == s->atlas_texture &&
                last->first_index + last->index_count == first_index) {
                last->index_count += static_cast<uint32_t>(s->num_indices);
            } else {
                runs.push_back(
                    TextRun{s->atlas_texture, first_index, static_cast<uint32_t>(s->num_indices)});
            }
        }
    }
}

void TextEngine::destroy(gpu::Device &device) {
    // The atlas chain belongs to SDL_ttf and dies with the engine below; only the objects are ours.
    (void)device;
    for (int face = 0; face < kFaceCount; ++face) {
        for (auto &pair : slots_[face]) {
            Slot &slot = pair.second;
            for (CachedText &entry : slot.texts) TTF_DestroyText(entry.object);
            slot.texts.clear();
            TTF_CloseFont(slot.font);
        }
        slots_[face].clear();
    }
    live_texts_ = 0;
    // Every TTF_Text is gone by now, which is the engine's own precondition for this call.
    if (engine_) TTF_DestroyGPUTextEngine(engine_);
    engine_ = nullptr;
}

}  // namespace opra
