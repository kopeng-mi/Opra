// Text: three shipped faces laid out by SDL_ttf's GPU text engine, which owns the glyph atlas,
// its packing and growth, the layout and the kerning. This layer adds what the HUD needs on top of
// it: a (face, px) font cache, the tabular digit grid the faces do not ship, and the frame's runs.
#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>

#include "gpu/gpu.h"
#include "ui/draw.h"

namespace opra {

/** One atlas texture's slice of the frame's text, drawn in one call against that atlas. */
struct TextRun {
    SDL_GPUTexture *texture;
    uint32_t first_index;
    uint32_t index_count;
};

class TextEngine {
public:
    /** Shipped font files, one per face. A missing or unopenable file is fatal, not silent. */
    void set_fonts(std::string display, std::string readout, std::string label);

    /**
     * Binds the device the drawing engine creates its atlas on. `measure` needs no device; `build`
     * before this is fatal, because there is no atlas to take glyph quads from.
     */
    void attach(gpu::Device &device);

    /** Pixel width of the string as `build` places it: digits on the grid, the rest natural. */
    float measure(const TextDraw &draw);

    /**
     * Appends every string's vertices to `vertices` and its (offset-adjusted) indices to
     * `indices`, and returns one run per atlas texture the frame needs. Screen space, y down,
     * `at` is the top-left of the text box before `TextDraw::align` is applied.
     */
    void build(const std::vector<TextDraw> &texts, std::vector<UIVertex> &vertices,
               std::vector<uint32_t> &indices, std::vector<TextRun> &runs);

    void destroy(gpu::Device &device);

private:
    /** One character on the pen walk: where it sits, which bytes encode it, and whether it draws. */
    struct GlyphCell {
        /** The glyph's own origin on the walk: the x `build` places the sub-draw at. */
        float pen = 0.0f;
        /** Byte range of the UTF-8 encoding, so a sub-draw is a slice of the caller's string. */
        uint32_t begin = 0;
        uint32_t end = 0;
        /** SDL_ttf emits a quad only for a glyph with a non-empty ink box. */
        bool draws = false;
    };

    /** One string's pen walk: the numbers `measure` and `build` share so they cannot disagree. */
    struct Layout {
        float width = 0.0f;
        /** Tallest ink above the baseline: the draw data's ascent line, pushed down when the
         *  tallest glyph stands above it. */
        int ink_top = 0;
        /** One line and every codepoint measured: what lets a readout split into per-character
         *  draws without a glyph escaping the walk. */
        bool splittable = false;
    };

    /** One laid-out string, reused while its text repeats and re-set when it does not. */
    struct CachedText {
        TTF_Text *object = nullptr;
        std::string text;
        /** Frame the entry was last asked for; the eviction order. */
        uint64_t used = 0;
    };

    /** One (face, px) pair: its font, its digit grid, and the strings held against it. */
    struct Slot {
        TTF_Font *font = nullptr;
        int ascent = 0;
        /** Widest digit advance; Readout only. 0 leaves the digits proportional. */
        int digit_advance = 0;
        std::vector<CachedText> texts;
    };

    Slot &slot_for(TextFace face, int px);
    /** Walks the string; fills one cell per codepoint when `cells` is asked for. */
    Layout lay_out(const Slot &slot, const std::string &text, std::vector<GlyphCell> *cells);
    TTF_Text *text_for(Slot &slot, std::string_view text);
    /** Frees the least recently used live text to stay under the cap. */
    void evict_text();

    /**
     * Draws one already-placed string: SDL_ttf's quads offset by `left`, with `top` carrying the
     * ascent-line push, and one run per atlas slice.
     */
    void append(Slot &slot, std::string_view text, float left, float top, const glm::vec4 &color,
                std::vector<UIVertex> &vertices, std::vector<uint32_t> &indices,
                std::vector<TextRun> &runs);

    static constexpr int kFaceCount = 3;
    std::string face_paths_[kFaceCount];
    std::unordered_map<int, Slot> slots_[kFaceCount];
    TTF_TextEngine *engine_ = nullptr;
    uint64_t frame_ = 0;
    int live_texts_ = 0;
    /** Reused by build: the readout being split, one cell per character. */
    std::vector<GlyphCell> cells_;
};

}  // namespace opra
