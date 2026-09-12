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
    // Tabular figures the faces do not ship (PLAN-02 s4.3), A5's refinement: the grid belongs to
    // the Readout face alone, where one draw per character places every digit on it. The other
    // faces keep their own advances and kerning. Measured per size, from the same cached glyphs
    // the split sub-draws are laid out with, so the grid and the ink cannot disagree.
    int widest = 0;
    if (index == 1) {
        for (uint32_t digit = '0'; digit <= '9'; ++digit) {
            int min_x = 0, max_x = 0, min_y = 0, max_y = 0, advance = 0;
            if (TTF_GetGlyphMetrics(slot.font, digit, &min_x, &max_x, &min_y, &max_y, &advance) &&
                advance > widest) {
                widest = advance;
            }
        }
    }
    slot.digit_advance = widest;
    return slot;
}

TextEngine::Layout TextEngine::lay_out(const Slot &slot, const std::string &text,
                                       std::vector<GlyphCell> *cells) {
    Layout layout;
    layout.splittable = true;
    if (cells) cells->clear();
    float pen = 0.0f;
    uint32_t previous = 0;
    for (size_t i = 0; i < text.size();) {
        const size_t begin = i;
        const uint32_t codepoint = next_codepoint(text, i);
        // A second line restarts both the pen and the ascent line, and a readout is one line.
        if (codepoint == '\n' || codepoint == '\r') layout.splittable = false;
        int min_x = 0, max_x = 0, min_y = 0, max_y = 0, advance = 0;
        if (!TTF_GetGlyphMetrics(slot.font, codepoint, &min_x, &max_x, &min_y, &max_y, &advance)) {
            // SDL_ttf still draws its .notdef for this codepoint; the walk no longer describes it.
            layout.splittable = false;
        }
        // Kerning, except across a digit: the grid is what keeps the columns in line, and a kerned
        // digit pair would break it as surely as a proportional advance would.
        if (previous && !is_digit(previous) && !is_digit(codepoint)) {
            int kerning = 0;
            if (TTF_GetGlyphKerning(slot.font, previous, codepoint, &kerning)) {
                pen += static_cast<float>(kerning);
            }
        }
        if (cells) {
            const bool draws = max_x > min_x && max_y > min_y;
            cells->push_back(GlyphCell{pen, static_cast<uint32_t>(begin), static_cast<uint32_t>(i),
                                       draws});
        }
        if (max_y > layout.ink_top) layout.ink_top = max_y;
        pen += static_cast<float>(slot.digit_advance > 0 && is_digit(codepoint) ? slot.digit_advance
                                                                               : advance);
        previous = codepoint;
    }
    layout.width = pen;
    return layout;
}

TTF_Text *TextEngine::text_for(Slot &slot, std::string_view text) {
    for (CachedText &entry : slot.texts) {
        if (std::string_view(entry.text) == text) {
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
        if (!TTF_SetTextString(reusable->object, text.data(), text.size())) {
            fatal("TTF_SetTextString");
        }
        reusable->text.assign(text.data(), text.size());
        reusable->used = frame_;
        return reusable->object;
    }
    if (live_texts_ >= kMaxTexts) evict_text();
    TTF_Text *object = TTF_CreateText(engine_, slot.font, text.data(), text.size());
    if (!object) fatal("TTF_CreateText");
    slot.texts.push_back(CachedText{object, std::string(text), frame_});
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
    return lay_out(slot_for(draw.face, px_of(draw.px)), draw.text, nullptr).width;
}

void TextEngine::append(Slot &slot, std::string_view text, float left, float top,
                        const glm::vec4 &color, std::vector<UIVertex> &vertices,
                        std::vector<uint32_t> &indices, std::vector<TextRun> &runs) {
    TTF_GPUAtlasDrawSequence *sequence = TTF_GetGPUTextDrawData(text_for(slot, text));
    if (!sequence) return;  // all whitespace: SDL_ttf hands back nothing to draw
    for (TTF_GPUAtlasDrawSequence *s = sequence; s; s = s->next) {
        if (!s->atlas_texture || !s->xy || !s->uv || !s->indices) {
            continue;  // a quad this frame cannot sample: never appended
        }
        const uint32_t first_vertex = static_cast<uint32_t>(vertices.size());
        const uint32_t first_index = static_cast<uint32_t>(indices.size());
        for (int i = 0; i < s->num_vertices; i += 4) {
            for (int v = 0; v < 4; ++v) {
                UIVertex vertex;
                vertex.pos = glm::vec2(s->xy[i + v].x + left, top - s->xy[i + v].y);
                vertex.uv = glm::vec2(s->uv[i + v].x, s->uv[i + v].y);
                vertex.color = color;
                vertices.push_back(vertex);
            }
            const int base = (i / 4) * 6;
            for (int v = 0; v < 6; ++v) {
                indices.push_back(first_vertex + static_cast<uint32_t>(s->indices[base + v]));
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

void TextEngine::build(const std::vector<TextDraw> &texts, std::vector<UIVertex> &vertices,
                       std::vector<uint32_t> &indices, std::vector<TextRun> &runs) {
    ++frame_;
    if (texts.empty()) return;
    if (!engine_) fatal("TextEngine::build");
    for (const TextDraw &draw : texts) {
        if (draw.text.empty()) continue;
        Slot &slot = slot_for(draw.face, px_of(draw.px));
        // A5: a readout splits into one draw per character, each at the pen the walk computed, so
        // every digit sits on the grid by construction and SDL_ttf is never asked to reconcile a
        // long string against itself - the old x-sort of its permuted quads is gone with the bug
        // it dropped. Labels draw whole and keep their kerning.
        const Layout layout = lay_out(slot, draw.text,
                                      draw.face == TextFace::Readout ? &cells_ : nullptr);
        const bool split = draw.face == TextFace::Readout && layout.splittable && !cells_.empty();
        float left = draw.at.x;
        if (draw.align == TextAlign::Center) {
            left -= layout.width * 0.5f;
        } else if (draw.align == TextAlign::Right) {
            left -= layout.width;
        }
        // xy is y-up and its origin is the ascent line, pushed down when the tallest glyph stands
        // above it: the baseline is at `top` plus the ascent. The string's tallest glyph sets that
        // push once, and every per-character sub-draw of the line shares it.
        const int ystart = std::max(0, layout.ink_top - slot.ascent);
        const float top = draw.at.y - static_cast<float>(ystart);
        if (split) {
            for (const GlyphCell &cell : cells_) {
                if (!cell.draws) continue;
                append(slot,
                       std::string_view(draw.text).substr(cell.begin, cell.end - cell.begin),
                       left + cell.pen, top, draw.color, vertices, indices, runs);
            }
        } else {
            append(slot, draw.text, left, top, draw.color, vertices, indices, runs);
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
