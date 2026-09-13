// Owns the GPU side of drawing: the mesh library's buffers, the instance buffer, the UI batch
// buffer, the pipelines and the texture sampler.
//
// One frame is a chain of passes (plan 4.1): a shadow map from the star, the scene into an HDR
// multisample target with reversed-Z depth, a resolve, the bloom chain, an ACES tonemap onto the
// swapchain, and the HUD last - on the swapchain, with no tonemap, because the UI is authored in
// display space and a tonemapped #DCE6E8 is not #DCE6E8.
#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include <SDL3/SDL.h>

#include "gpu/gpu.h"
#include "render/camera.h"
#include "render/material.h"
#include "render/model.h"
#include "render/scene.h"
#include "render/text.h"
#include "ui/draw.h"

namespace opra {

/** The scene's colour target: 16-bit float, so the star and the drive flames can be brighter than
 *  white and the bloom chain has something to find. */
inline constexpr SDL_GPUTextureFormat HDR_FORMAT = SDL_GPU_TEXTUREFORMAT_R16G16B16A16_FLOAT;

/** Bloom levels: threshold, five downsamples, five tent upsamples (plan 4.1). */
inline constexpr int BLOOM_MIPS = 5;

/** The shadow cascade's edge, texels. One cascade, fitted to the follow camera (plan 4.1). */
inline constexpr Uint32 SHADOW_SIZE = 2048;

struct PipelineSet {
    SDL_GPUTextureFormat format = SDL_GPU_TEXTUREFORMAT_INVALID;
    Uint32 samples = 1;
    SDL_GPUGraphicsPipeline *mesh = nullptr;
    /** Additive, no depth write, exhaust profile on alpha: flames, jets and glow. */
    SDL_GPUGraphicsPipeline *effect = nullptr;
    /** Additive, no depth write, instance alpha untouched: the sky and the grit. */
    SDL_GPUGraphicsPipeline *backdrop = nullptr;
    /** Planets and the star: one sphere plus a shader, LOD by angular size. */
    SDL_GPUGraphicsPipeline *planet = nullptr;
    SDL_GPUGraphicsPipeline *star = nullptr;
    /** A planet's air: the same sphere at the shell radius, additive, no depth write. */
    SDL_GPUGraphicsPipeline *air = nullptr;
    /** Depth only, into the 2048² cascade. */
    SDL_GPUGraphicsPipeline *shadow = nullptr;
    SDL_GPUGraphicsPipeline *bloom_threshold = nullptr;
    SDL_GPUGraphicsPipeline *bloom_downsample = nullptr;
    SDL_GPUGraphicsPipeline *bloom_upsample = nullptr;
    SDL_GPUGraphicsPipeline *bloom_combine = nullptr;
    /** ACES + grade, writing the swapchain. */
    SDL_GPUGraphicsPipeline *tonemap = nullptr;
    SDL_GPUGraphicsPipeline *ui = nullptr;
};

/** Mirrors the cbuffer in shaders/mesh.hlsl: field order is the register layout. */
struct MeshUniforms {
    glm::mat4 view_proj;
    glm::vec4 light_dir;  // xyz: toward the star, w: ambient floor
    glm::vec4 tint;       // rgb: ambient tint
    glm::vec4 view_dir;   // xyz: camera forward, w: rim strength
    glm::vec4 fill_dir;   // xyz: toward the fill light, w: fill strength
    glm::mat4 light_view_proj;
    glm::vec4 light_color;    // rgb: the star's colour, w: its intensity
    glm::vec4 shadow_params;  // x: texel size in metres, y: depth bias, z: 1 inside, w: unused
};

/** Mirrors the material cbuffer: the factors and which texture slots are actually bound. */
struct MaterialUniforms {
    glm::vec4 base_color;          // rgb factor, a unused
    glm::vec4 metallic_roughness;  // x metallic, y roughness, z triplanar
    glm::vec4 texture_flags;       // x base colour, y metallic-roughness, z normal, w unlit
};

/** Mirrors the cbuffer in shaders/planet.hlsl and shaders/star.hlsl. */
struct BodyUniforms {
    glm::mat4 view_proj;
    glm::vec4 center_radius;    // xyz centre, w drawn radius
    glm::vec4 color;            // rgb albedo, a unused
    glm::vec4 terrain;          // x seed, y amplitude, z class, w cloud clock
    glm::vec4 atmosphere;       // x scale height, y top, z star angular radius, w unused
    glm::vec4 star_direction;   // xyz toward the star, w intensity
    glm::vec4 star_color;       // rgb
    glm::vec4 viewport;         // xy pixels, z star pixel radius, w body pixel radius
    glm::vec4 eye_position;     // xyz the camera eye, w unused
    glm::vec4 maps;             // x albedo, y clouds, z night (or star photosphere), w unused
};

/** Mirrors the cbuffer in shaders/bloom.hlsl: the source and target texel sizes, and the chain's
 *  two tuning numbers. */
struct BloomUniforms {
    glm::vec4 texel;   // xy source texel, zw target texel
    glm::vec4 params;  // x threshold, y knee, z upsample strength, w unused
};

/** Mirrors the cbuffer in shaders/tonemap.hlsl. */
struct GradeUniforms {
    glm::vec4 params;  // x exposure, y bloom, z saturation, w contrast
};

struct UIUniforms {
    glm::vec2 screen;
    glm::vec2 pad;
};

/**
 * One sky body as this frame draws it: the instance, the body uniform, and which of the three
 * pipelines it belongs to. Built once per frame from the angular size, so the shell and the star
 * share the numbers the surface already worked out.
 */
struct SkyDraw {
    Instance instance;
    BodyUniforms uniforms;
    int mesh = -1;
    bool shell = false;
    bool star = false;
    /** The re-projected shell set (plan 05 s2.3): drawn after the near pass's backdrop. */
    bool deep = false;
    /** The bound maps for this draw, matching the shader's t0..t2; null samples the white texel. */
    SDL_GPUTexture *maps[3] = {nullptr, nullptr, nullptr};
    bool tile = false;  // the star's photosphere samples a tile sampler; planets, equirectangular
};

struct Renderer {
    gpu::Device device;
    std::vector<PipelineSet> pipelines;

    SDL_GPUBuffer *mesh_vertices = nullptr;
    SDL_GPUBuffer *mesh_indices = nullptr;
    std::vector<GpuMesh> meshes;
    /** One per library mesh, deduplicated: what each run binds before its draws. */
    MaterialTable materials;
    /** PBR textures by the material's slots. The current exports carry no image data, so this is
     *  empty and every material samples the white texel with its texture flag clear. */
    std::vector<SDL_GPUTexture *> textures;

    /** Grown in place, one transfer buffer each, written inside the frame's copy pass. */
    gpu::DynamicBuffer instances;
    /** One Instance per sky body: the planet and star shaders read the same layout as the mesh
     *  pass, so there is no second vertex format. */
    gpu::DynamicBuffer body_instances;
    gpu::DynamicBuffer ui_vertices;
    /** The frame's UI geometry, reused every frame: the solids' triangle soup, then every string. */
    std::vector<UIVertex> ui_vertex_scratch;
    std::vector<uint32_t> ui_index_scratch;
    std::vector<TextRun> text_runs;
    /** The frame's UI index buffer, grown by the same rule as the vertex buffers. gpu::DynamicBuffer
     *  only makes VERTEX buffers, and a vertex buffer cannot be bound as an index buffer. */
    SDL_GPUBuffer *ui_indices = nullptr;
    SDL_GPUTransferBuffer *ui_index_transfer = nullptr;
    Uint32 ui_index_capacity = 0;
    /** 1x1 opaque white: the UI shader always samples, and a material with no texture of its own
     *  samples this, so the sampler bindings are never holes. */
    SDL_GPUTexture *white = nullptr;
    /** Reused every frame: the sort writes into these instead of allocating. */
    std::vector<Instance> sorted_instances;
    /**
     * The sky bodies' maps, loaded from assets/textures/manifest.json and keyed by the body's
     * own map names (plan-04 s3.4). An unknown name is a null texture: the body keeps its
     * procedural shading rather than crashing.
     */
    std::unordered_map<std::string, SDL_GPUTexture *> body_maps;
    std::vector<SDL_GPUTexture *> body_map_textures;  // every handle, for teardown
    bool body_manifest_read = false;
    /** Equirectangular maps wrap in longitude and clamp at the poles; tiles wrap in both axes. */
    SDL_GPUSampler *map_sampler_equirect = nullptr;
    SDL_GPUSampler *map_sampler_tile = nullptr;
    /** Resolves a map name to its texture, reading the manifest on the first miss. */
    SDL_GPUTexture *map_texture(const std::string &name);
    /** The frame's runs, opaque first, then effects, then the backdrop: the draw order. */
    std::vector<InstanceRun> opaque_runs;
    std::vector<InstanceRun> effect_runs;
    std::vector<InstanceRun> backdrop_runs;
    /** This frame's sky bodies, in the order they are drawn: surfaces, then air, then stars. */
    std::vector<Instance> sky_instances;
    std::vector<SkyDraw> sky_draws;
    /**
     * Frames drawn since startup. The render layer has no sim clock - draw_frame's signature is the
     * frame's contract with the game layer - and a cloud deck that used the wall clock would make
     * two frames of the same instant differ. A count is deterministic for a frame sequence, which
     * is what the golden images need (plan 5.2).
     */
    Uint64 frame_index = 0;
    Uint32 instance_count = 0;
    Uint32 run_count = 0;
    Uint64 triangle_count = 0;

    SDL_GPUSampler *sampler = nullptr;
    /** One sampler for the shadow map's point taps: a linear tap would blend two depths into a
     *  surface that is not there. */
    SDL_GPUSampler *shadow_sampler = nullptr;

    SDL_GPUTexture *depth = nullptr;
    Uint32 depth_width = 0;
    Uint32 depth_height = 0;
    /** Sample count the depth target was built with: the settings can change it. */
    Uint32 depth_samples = 0;

    /** This frame's shadow cascade, and the matrix that put it there. */
    SDL_GPUTexture *shadow_map = nullptr;
    glm::mat4 shadow_view_proj{1.0f};

    /** The scene target, and the multisample target it is resolved from when samples > 1. */
    SDL_GPUTexture *hdr_color = nullptr;
    Uint32 hdr_width = 0;
    Uint32 hdr_height = 0;
    SDL_GPUTexture *hdr_msaa = nullptr;
    Uint32 msaa_width = 0;
    Uint32 msaa_height = 0;
    Uint32 msaa_samples = 0;

    /** The bloom chain's five levels, each half the one before, and the graded composite. */
    SDL_GPUTexture *bloom_mips[BLOOM_MIPS] = {};
    Uint32 bloom_width[BLOOM_MIPS] = {};
    Uint32 bloom_height[BLOOM_MIPS] = {};
    SDL_GPUTexture *hdr_lit = nullptr;
    Uint32 lit_width = 0;
    Uint32 lit_height = 0;

    /** Multisample count for this session, from the settings. Changing it rebuilds pipelines. */
    Uint32 samples = 1;

    /**
     * Pipelines are per (target format, sample count): the swapchain format changes on HDR
     * monitors, and the settings can turn multisampling on or off at runtime.
     */
    const PipelineSet &pipelines_for(SDL_GPUTextureFormat format, Uint32 samples);
};

/** Grows the depth target when the drawable size changes. */
void ensure_depth(Renderer &renderer, Uint32 width, Uint32 height);

/** Merges every library mesh into one vertex/index pair and records their ranges and materials. */
void upload_mesh_library(Renderer &renderer, const MeshLibrary &library);

void destroy_renderer(Renderer &renderer);

/**
 * One frame: shadow, scene, resolve, bloom, tonemap, then the HUD pass (solid marks and text runs)
 * straight onto the swapchain. Owns every GPU call the frame needs, so nothing above this layer
 * names SDL_GPU. An empty scene or an empty HUD is a legal frame: the swapchain is still cleared
 * and tonemapped.
 */
void draw_frame(Renderer &renderer, TextEngine &text, SDL_GPUCommandBuffer *cmd,
                SDL_GPUTexture *color, SDL_GPUTextureFormat format, Uint32 width, Uint32 height,
                const Camera &camera, SceneBuilder &scene, UIBatch &ui);

}  // namespace opra
