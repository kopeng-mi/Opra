// Assertions for the text engine: the tabular digit grid the shipped faces do not have, and the
// frame's index bookkeeping. Run through `Opra.exe --selftest`.
//
// The expected widths are not captured from this run: they come from SDL_ttf's own metrics API and
// from the rule PLAN-02 §4.3 states — digits take the widest digit's advance, everything else its
// own, kerning between two non-digits — written out independently of the engine below.
#include "selftest.h"

#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

#include "core/file.h"
#include "gpu/gpu.h"
#include "render/text.h"

namespace opra {
namespace {

int g_failures = 0;

/** Forwards to the game's harness and counts here: text_tests() returns its own failure count. */
void check(bool ok, const char *what) {
    selftest::check(ok, what);
    if (!ok) ++g_failures;
}

void check_close(double actual, double expected, double tolerance, const char *what) {
    selftest::check_close(actual, expected, tolerance, what);
    if (!(std::fabs(actual - expected) <= tolerance)) ++g_failures;
}

struct FaceCase {
    TextFace face;
    const char *path;
    float px;
};

// The plan's sizes (§4.3): Display 50/32, Readout 25/20/16/13, Label 13/10.
const FaceCase kCases[8] = {
    {TextFace::Display, "assets/fonts/HydrogenWhiskey.otf", 50.0f},
    {TextFace::Display, "assets/fonts/HydrogenWhiskey.otf", 32.0f},
    {TextFace::Readout, "assets/fonts/BarlowCondensed-SemiBold.ttf", 25.0f},
    {TextFace::Readout, "assets/fonts/BarlowCondensed-SemiBold.ttf", 20.0f},
    {TextFace::Readout, "assets/fonts/BarlowCondensed-SemiBold.ttf", 16.0f},
    {TextFace::Readout, "assets/fonts/BarlowCondensed-SemiBold.ttf", 13.0f},
    {TextFace::Label, "assets/fonts/Barlow-Regular.ttf", 13.0f},
    {TextFace::Label, "assets/fonts/Barlow-Regular.ttf", 10.0f},
};

TextDraw draw_of(TextFace face, float px, const char *text) {
    TextDraw draw;
    draw.text = text;
    draw.px = px;
    draw.face = face;
    return draw;
}

bool is_digit(uint32_t codepoint) { return codepoint >= '0' && codepoint <= '9'; }

/** The widest digit advance at this size: the grid every digit is laid on. */
int digit_grid(TTF_Font *font) {
    int widest = 0;
    for (uint32_t digit = '0'; digit <= '9'; ++digit) {
        int min_x = 0, max_x = 0, min_y = 0, max_y = 0, advance = 0;
        if (TTF_GetGlyphMetrics(font, digit, &min_x, &max_x, &min_y, &max_y, &advance) &&
            advance > widest) {
            widest = advance;
        }
    }
    return widest;
}

/** PLAN-02 §4.3, written out: ASCII only, which is every string this UI draws today. */
float expected_width(TTF_Font *font, const char *text, int grid) {
    float pen = 0.0f;
    uint32_t previous = 0;
    for (const char *at = text; *at; ++at) {
        const uint32_t codepoint = static_cast<unsigned char>(*at);
        int min_x = 0, max_x = 0, min_y = 0, max_y = 0, advance = 0;
        TTF_GetGlyphMetrics(font, codepoint, &min_x, &max_x, &min_y, &max_y, &advance);
        if (previous && !is_digit(previous) && !is_digit(codepoint)) {
            int kerning = 0;
            if (TTF_GetGlyphKerning(font, previous, codepoint, &kerning)) {
                pen += static_cast<float>(kerning);
            }
        }
        pen += static_cast<float>(is_digit(codepoint) ? grid : advance);
        previous = codepoint;
    }
    return pen;
}

/** The grid every digit is laid on, per (face, size), read through the public API. */
void test_digit_grid(TextEngine &engine) {
    for (const FaceCase &test : kCases) {
        const std::string path = asset_path(test.path);
        TTF_Font *font = TTF_OpenFont(path.c_str(), test.px);
        if (!font) {
            check(false, "text: the shipped face opens at the plan's size");
            continue;
        }
        const int grid = digit_grid(font);
        check(grid > 0, "text: the widest digit has an advance");

        // The plan's own acceptance case (§4.3): a value counting 199 -> 200 must not move.
        const float zeros = engine.measure(draw_of(test.face, test.px, "0000"));
        const float ones = engine.measure(draw_of(test.face, test.px, "1111"));
        check(zeros == ones, "text: 0000 and 1111 measure the same");
        check_close(zeros, 4.0 * grid, 1e-6, "text: four digits are four grid advances");

        const float one_nine_nine = engine.measure(draw_of(test.face, test.px, "199"));
        const float two_hundred = engine.measure(draw_of(test.face, test.px, "200"));
        check(one_nine_nine == two_hundred, "text: 199 and 200 measure the same");
        check_close(one_nine_nine, 3.0 * grid, 1e-6, "text: three digits are three grid advances");

        const float four = engine.measure(draw_of(test.face, test.px, "1999"));
        const float five = engine.measure(draw_of(test.face, test.px, "19999"));
        check(five > four, "text: a fifth digit adds width");

        // Digits and letters in one string: the digits on the grid, the letters on their own
        // advances, nothing double-counted at the joins.
        const char *mixed = "31.7 m/s";
        check_close(engine.measure(draw_of(test.face, test.px, mixed)),
                    expected_width(font, mixed, grid), 1e-4, "text: digits and letters mix");
        // A string with no digits is untouched by the grid.
        const char *plain = "Kestrel";
        check_close(engine.measure(draw_of(test.face, test.px, plain)),
                    expected_width(font, plain, grid), 1e-4, "text: a non-digit string keeps its advances");

        TTF_CloseFont(font);
    }
}

/**
 * The frame's one index buffer: the solids' trivial run first, then a run per atlas. SDL_ttf hands
 * back four vertices per quad; build() must append all four and index them from the run's own
 * vertex base - an offset applied twice, or not at all, shows up as an index past the end.
 */
void test_build_bookkeeping() {
    SDL_GPUDevice *handle = SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_DXIL, false, nullptr);
    if (!handle) {
        // The drawing engine lives on the device; there is no software path to check it against.
        check(false, "text: a GPU device is needed to exercise build()");
        return;
    }
    gpu::Device device;
    device.handle = handle;
    TextEngine engine;
    engine.set_fonts(asset_path("assets/fonts/HydrogenWhiskey.otf"),
                     asset_path("assets/fonts/BarlowCondensed-SemiBold.ttf"),
                     asset_path("assets/fonts/Barlow-Regular.ttf"));
    engine.attach(device);

    std::vector<TextDraw> texts;
    texts.push_back(draw_of(TextFace::Readout, 20.0f, "31.7 m/s"));
    texts.push_back(draw_of(TextFace::Label, 13.0f, "Kestrel"));
    texts.push_back(draw_of(TextFace::Readout, 20.0f, "199"));

    std::vector<UIVertex> vertices(3);  // stand-in for the frame's solids
    std::vector<uint32_t> indices{0, 1, 2};
    std::vector<TextRun> runs;
    engine.build(texts, vertices, indices, runs);

    check(indices.size() >= 3 && indices[0] == 0 && indices[1] == 1 && indices[2] == 2,
          "text: the solids' sequential indices are left alone");
    check(!runs.empty(), "text: build reports at least one run");
    check(indices.size() > 3, "text: build appended the strings' indices");

    uint32_t next_first = 3;
    uint32_t referenced = 0;  // highest vertex index any run reaches
    bool consistent = true;
    for (const TextRun &run : runs) {
        if (run.texture == nullptr || run.first_index != next_first || run.index_count == 0 ||
            run.index_count % 3 != 0 || run.first_index + run.index_count > indices.size()) {
            consistent = false;
            continue;
        }
        uint32_t lowest = 0xFFFFFFFFu;
        for (uint32_t i = 0; i < run.index_count; ++i) {
            const uint32_t index = indices[run.first_index + i];
            if (index >= vertices.size()) consistent = false;
            if (index < lowest) lowest = index;
            if (index > referenced) referenced = index;
        }
        // Four vertices per six indices, referenced from the run's own base, contiguously: this is
        // the whole offset contract between build() and the draw calls in draw_frame.
        if (lowest != next_first || referenced + 1 != lowest + 4 * (run.index_count / 6)) {
            consistent = false;
        }
        next_first += run.index_count;
    }
    check(consistent, "text: every run indexes a contiguous block of the frame's vertices");
    check(next_first == indices.size(), "text: the runs cover every index after the solids");
    check(referenced + 1 == vertices.size(), "text: build appends no vertex its indices do not use");
    // The grid is a placement, not only a measurement: four '1's in the narrowest face must sit one
    // grid advance apart, where SDL_ttf's natural layout would put them on their 5 px advance.
    TTF_Font *readout = TTF_OpenFont(asset_path("assets/fonts/BarlowCondensed-SemiBold.ttf").c_str(),
                                     20.0f);
    check(readout != nullptr, "text: the readout face opens at 20 px");
    const float grid = readout ? static_cast<float>(digit_grid(readout)) : 0.0f;
    int digit_left = 0, digit_right = 0, digit_bottom = 0, digit_top = 0, digit_advance = 0;
    TTF_GetGlyphMetrics(readout, '1', &digit_left, &digit_right, &digit_bottom, &digit_top,
                        &digit_advance);
    const float ascent = static_cast<float>(TTF_GetFontAscent(readout));

    std::vector<TextDraw> ones_text;
    TextDraw ones = draw_of(TextFace::Readout, 20.0f, "1111");
    ones.at = {50.0f, 50.0f};
    ones_text.push_back(ones);
    std::vector<UIVertex> one_vertices(3);
    std::vector<uint32_t> one_indices{0, 1, 2};
    std::vector<TextRun> one_runs;
    engine.build(ones_text, one_vertices, one_indices, one_runs);
    check(one_vertices.size() == 3 + 16, "text: four glyphs append four quads");
    // SDL_ttf hands the quads back sorted by atlas, not by string order, so the check sorts them
    // itself: four '1's must sit one grid advance apart, where their own advance is 5 px.
    float quad_x[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    for (int quad = 0; quad < 4; ++quad) {
        quad_x[quad] = one_vertices[3 + static_cast<size_t>(quad) * 4].pos.x;
    }
    std::sort(quad_x, quad_x + 4);
    for (int quad = 0; quad + 1 < 4; ++quad) {
        check_close(quad_x[quad + 1] - quad_x[quad], grid, 0.01,
                    "text: consecutive '1's sit on the grid, not their advance");
    }
    // The y anchor is the engine this replaced: `at.y` is the top of the ascent line, so a '1'
    // whose ink reaches 15 px above the baseline lands with its top at at.y + ascent - 15 and its
    // foot on the baseline at at.y + ascent. SDL_ttf's draw data is y-up from that same line.
    float ink_top = 1.0e9f, ink_bottom = -1.0e9f;
    for (size_t v = 3; v < one_vertices.size(); ++v) {
        ink_top = std::min(ink_top, one_vertices[v].pos.y);
        ink_bottom = std::max(ink_bottom, one_vertices[v].pos.y);
    }
    check_close(ink_top, 50.0 + ascent - static_cast<double>(digit_top), 0.01,
                "text: the glyph top sits where the baseline rule puts it");
    check_close(ink_bottom, 50.0 + ascent, 0.01, "text: the glyph foot sits on the baseline");
    TTF_CloseFont(readout);
    check_close(engine.measure(draw_of(TextFace::Readout, 20.0f, "1111")), 4.0 * grid, 1e-6,
                "text: the grid places what the grid measures");

    engine.destroy(device);
    SDL_DestroyGPUDevice(handle);
}

}  // namespace

int text_tests() {
    g_failures = 0;
    // The rest of the suite is headless and starts none of this; text is the one module whose
    // subject is a device-backed atlas, so it brings up what it uses and takes it down again.
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        check(false, "text: SDL_Init");
        return g_failures;
    }
    if (!TTF_Init()) {
        check(false, "text: TTF_Init");
        SDL_Quit();
        return g_failures;
    }
    {
        TextEngine engine;
        engine.set_fonts(asset_path("assets/fonts/HydrogenWhiskey.otf"),
                         asset_path("assets/fonts/BarlowCondensed-SemiBold.ttf"),
                         asset_path("assets/fonts/Barlow-Regular.ttf"));
        test_digit_grid(engine);
        // measure() opened fonts and no atlas; the device is only what destroy() never needed.
        gpu::Device no_device;
        engine.destroy(no_device);
    }
    test_build_bookkeeping();
    TTF_Quit();
    SDL_Quit();
    return g_failures;
}

}  // namespace opra
