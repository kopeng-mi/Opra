#include "render/renderer.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>

#include <glm/gtc/matrix_transform.hpp>
#include <nlohmann/json.hpp>

#include "core/log.h"
#include "gpu/gpu.h"
#include "render/mesh.h"
#include "render/texture.h"

namespace opra {

// The body maps' manifest, read like every other file's json (gltf.cpp, system.cpp).
using json = nlohmann::json;

namespace {

/** SDL's sample count for a renderer configured with `samples`. */
SDL_GPUSampleCount sample_count_for(Uint32 samples) {
    return samples >= 4 ? SDL_GPU_SAMPLECOUNT_4 : SDL_GPU_SAMPLECOUNT_1;
}

// ------------------------------------------------------------------ pipeline

SDL_GPUShader *mesh_shader(SDL_GPUDevice *dev, const char *path, const char *entry,
                           SDL_GPUShaderStage stage, Uint32 uniform_buffers, Uint32 samplers) {
    return gpu::make_shader(dev, path, stage, entry, uniform_buffers, samplers);
}

/**
 * The mesh family: the opaque PBR hulls, the additive effects (flames and jets, with the exhaust
 * profile on alpha) and the additive backdrop. All three share the vertex stage and the instance
 * layout, so one buffer binding serves every run in the scene pass.
 */
SDL_GPUGraphicsPipeline *create_mesh_pipeline(SDL_GPUDevice *dev, Uint32 samples, bool additive,
                                              bool flame_fade) {
    SDL_GPUShader *vs =
        mesh_shader(dev, flame_fade ? "shaders/mesh_vs_flame.dxil" : "shaders/mesh_vs.dxil",
                    flame_fade ? "VSFlame" : "VSMain", SDL_GPU_SHADERSTAGE_VERTEX, 1, 0);
    SDL_GPUShader *ps =
        additive
            ? mesh_shader(dev, "shaders/mesh_ps_ambient.dxil", "PSAmbient",
                          SDL_GPU_SHADERSTAGE_FRAGMENT, 1, 0)
            : mesh_shader(dev, "shaders/mesh_ps.dxil", "PSMain", SDL_GPU_SHADERSTAGE_FRAGMENT, 2,
                          4);

    SDL_GPUVertexBufferDescription buffers[2]{};
    buffers[0].slot = 0;
    buffers[0].pitch = sizeof(MeshVertex);
    buffers[0].input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;
    buffers[1].slot = 1;
    buffers[1].pitch = sizeof(Instance);
    buffers[1].input_rate = SDL_GPU_VERTEXINPUTRATE_INSTANCE;

    SDL_GPUVertexAttribute attributes[9]{};
    attributes[0] = {0, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3, offsetof(MeshVertex, pos)};
    attributes[1] = {1, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3, offsetof(MeshVertex, normal)};
    attributes[2] = {2, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4, offsetof(MeshVertex, tangent)};
    attributes[3] = {3, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2, offsetof(MeshVertex, uv)};
    attributes[4] = {4, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3, offsetof(MeshVertex, color)};
    attributes[5] = {5, 1, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4, offsetof(Instance, pos)};
    attributes[6] = {6, 1, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4, offsetof(Instance, rot)};
    attributes[7] = {7, 1, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4, offsetof(Instance, scale)};
    attributes[8] = {8, 1, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4, offsetof(Instance, color)};

    SDL_GPUVertexInputState vertex_input{};
    vertex_input.vertex_buffer_descriptions = buffers;
    vertex_input.num_vertex_buffers = 2;
    vertex_input.vertex_attributes = attributes;
    vertex_input.num_vertex_attributes = 9;

    SDL_GPURasterizerState rasterizer{};
    rasterizer.fill_mode = SDL_GPU_FILLMODE_FILL;
    // Cones are single-sided and open at the mouth, so the effect pass draws whatever faces it has.
    rasterizer.cull_mode = additive ? SDL_GPU_CULLMODE_NONE : SDL_GPU_CULLMODE_BACK;
    rasterizer.front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;
    rasterizer.enable_depth_clip = true;

    SDL_GPUDepthStencilState depth{};
    // Reversed-Z, matching the projection: near is 1 and far is 0, so "closer" is greater and the
    // depth target clears to 0. The scene reaches 12 km while the hulls live inside 200 m of the
    // camera, and a conventional buffer spends its precision at the wrong end of that range.
    depth.compare_op = SDL_GPU_COMPAREOP_GREATER;
    depth.enable_depth_test = true;
    // Effects are tested against the hulls but never occlude each other.
    depth.enable_depth_write = additive == false;

    SDL_GPUColorTargetBlendState blend{};
    blend.enable_blend = additive;
    if (additive) {
        blend.src_color_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA;
        blend.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
        blend.color_blend_op = SDL_GPU_BLENDOP_ADD;
        blend.src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
        blend.dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
        blend.alpha_blend_op = SDL_GPU_BLENDOP_ADD;
    }

    SDL_GPUColorTargetDescription target{};
    target.format = HDR_FORMAT;
    target.blend_state = blend;

    SDL_GPUGraphicsPipelineCreateInfo info{};
    info.vertex_shader = vs;
    info.fragment_shader = ps;
    info.vertex_input_state = vertex_input;
    info.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
    info.rasterizer_state = rasterizer;
    info.multisample_state.sample_count = sample_count_for(samples);
    info.depth_stencil_state = depth;
    info.target_info.color_target_descriptions = &target;
    info.target_info.num_color_targets = 1;
    info.target_info.has_depth_stencil_target = true;
    info.target_info.depth_stencil_format = SDL_GPU_TEXTUREFORMAT_D32_FLOAT;

    SDL_GPUGraphicsPipeline *pipeline = SDL_CreateGPUGraphicsPipeline(dev, &info);
    if (!pipeline) fatal(additive ? "SDL_CreateGPUGraphicsPipeline (effect)"
                                  : "SDL_CreateGPUGraphicsPipeline (mesh)");
    SDL_ReleaseGPUShader(dev, vs);
    SDL_ReleaseGPUShader(dev, ps);
    return pipeline;
}

/**
 * The star's cascade: depth only, into the 2048² D32 map. Its own depth convention - DEPTH test,
 * clear to 1 - because a cascade is sampled as a plain value and not through the reversed-Z
 * projection the camera uses.
 */
SDL_GPUGraphicsPipeline *create_shadow_pipeline(SDL_GPUDevice *dev) {
    SDL_GPUShader *vs = mesh_shader(dev, "shaders/shadow_vs.dxil", "VSMain",
                                    SDL_GPU_SHADERSTAGE_VERTEX, 1, 0);
    SDL_GPUShader *ps = mesh_shader(dev, "shaders/shadow_ps.dxil", "PSMain",
                                    SDL_GPU_SHADERSTAGE_FRAGMENT, 0, 0);

    SDL_GPUVertexBufferDescription buffers[2]{};
    buffers[0].slot = 0;
    buffers[0].pitch = sizeof(MeshVertex);
    buffers[0].input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;
    buffers[1].slot = 1;
    buffers[1].pitch = sizeof(Instance);
    buffers[1].input_rate = SDL_GPU_VERTEXINPUTRATE_INSTANCE;

    SDL_GPUVertexAttribute attributes[4]{};
    attributes[0] = {0, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3, offsetof(MeshVertex, pos)};
    attributes[1] = {5, 1, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4, offsetof(Instance, pos)};
    attributes[2] = {6, 1, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4, offsetof(Instance, rot)};
    attributes[3] = {7, 1, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4, offsetof(Instance, scale)};

    SDL_GPUVertexInputState vertex_input{};
    vertex_input.vertex_buffer_descriptions = buffers;
    vertex_input.num_vertex_buffers = 2;
    vertex_input.vertex_attributes = attributes;
    vertex_input.num_vertex_attributes = 4;

    SDL_GPURasterizerState rasterizer{};
    rasterizer.fill_mode = SDL_GPU_FILLMODE_FILL;
    rasterizer.cull_mode = SDL_GPU_CULLMODE_BACK;
    rasterizer.front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;
    rasterizer.enable_depth_clip = true;

    SDL_GPUDepthStencilState depth{};
    depth.compare_op = SDL_GPU_COMPAREOP_LESS;
    depth.enable_depth_test = true;
    depth.enable_depth_write = true;

    SDL_GPUGraphicsPipelineCreateInfo info{};
    info.vertex_shader = vs;
    info.fragment_shader = ps;
    info.vertex_input_state = vertex_input;
    info.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
    info.rasterizer_state = rasterizer;
    info.multisample_state.sample_count = SDL_GPU_SAMPLECOUNT_1;
    info.depth_stencil_state = depth;
    info.target_info.num_color_targets = 0;
    info.target_info.has_depth_stencil_target = true;
    info.target_info.depth_stencil_format = SDL_GPU_TEXTUREFORMAT_D32_FLOAT;

    SDL_GPUGraphicsPipeline *pipeline = SDL_CreateGPUGraphicsPipeline(dev, &info);
    if (!pipeline) fatal("SDL_CreateGPUGraphicsPipeline (shadow)");
    SDL_ReleaseGPUShader(dev, vs);
    SDL_ReleaseGPUShader(dev, ps);
    return pipeline;
}

/**
 * A sky body: the planet surface, the planet's air shell or the star. All three read the same body
 * uniform and the same instance layout, and differ only in their fragment stage and blend state.
 */
SDL_GPUGraphicsPipeline *create_body_pipeline(SDL_GPUDevice *dev, Uint32 samples,
                                              const char *vs_path, const char *vs_entry,
                                              const char *ps_path, const char *ps_entry,
                                              bool additive, bool write_depth,
                                              Uint32 ps_textures = 0) {
    SDL_GPUShader *vs =
        mesh_shader(dev, vs_path, vs_entry, SDL_GPU_SHADERSTAGE_VERTEX, 1, 0);
    SDL_GPUShader *ps =
        mesh_shader(dev, ps_path, ps_entry, SDL_GPU_SHADERSTAGE_FRAGMENT, 1, ps_textures);

    SDL_GPUVertexBufferDescription buffers[2]{};
    buffers[0].slot = 0;
    buffers[0].pitch = sizeof(MeshVertex);
    buffers[0].input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;
    buffers[1].slot = 1;
    buffers[1].pitch = sizeof(Instance);
    buffers[1].input_rate = SDL_GPU_VERTEXINPUTRATE_INSTANCE;

    SDL_GPUVertexAttribute attributes[4]{};
    attributes[0] = {0, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3, offsetof(MeshVertex, pos)};
    attributes[1] = {5, 1, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4, offsetof(Instance, pos)};
    attributes[2] = {6, 1, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4, offsetof(Instance, rot)};
    attributes[3] = {7, 1, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4, offsetof(Instance, scale)};

    SDL_GPUVertexInputState vertex_input{};
    vertex_input.vertex_buffer_descriptions = buffers;
    vertex_input.num_vertex_buffers = 2;
    vertex_input.vertex_attributes = attributes;
    vertex_input.num_vertex_attributes = 4;

    SDL_GPURasterizerState rasterizer{};
    rasterizer.fill_mode = SDL_GPU_FILLMODE_FILL;
    // The air shell and the corona are both hemispheres seen from outside; culling the back face
    // keeps each pixel covered once, which matters for an additive pass.
    rasterizer.cull_mode = SDL_GPU_CULLMODE_BACK;
    rasterizer.front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;
    rasterizer.enable_depth_clip = true;

    SDL_GPUDepthStencilState depth{};
    depth.compare_op = SDL_GPU_COMPAREOP_GREATER;  // reversed-Z, like the scene it sits in
    depth.enable_depth_test = true;
    depth.enable_depth_write = write_depth;

    SDL_GPUColorTargetBlendState blend{};
    blend.enable_blend = additive;
    if (additive) {
        blend.src_color_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA;
        blend.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
        blend.color_blend_op = SDL_GPU_BLENDOP_ADD;
        blend.src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
        blend.dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
        blend.alpha_blend_op = SDL_GPU_BLENDOP_ADD;
    }

    SDL_GPUColorTargetDescription target{};
    target.format = HDR_FORMAT;
    target.blend_state = blend;

    SDL_GPUGraphicsPipelineCreateInfo info{};
    info.vertex_shader = vs;
    info.fragment_shader = ps;
    info.vertex_input_state = vertex_input;
    info.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
    info.rasterizer_state = rasterizer;
    info.multisample_state.sample_count = sample_count_for(samples);
    info.depth_stencil_state = depth;
    info.target_info.color_target_descriptions = &target;
    info.target_info.num_color_targets = 1;
    info.target_info.has_depth_stencil_target = true;
    info.target_info.depth_stencil_format = SDL_GPU_TEXTUREFORMAT_D32_FLOAT;

    SDL_GPUGraphicsPipeline *pipeline = SDL_CreateGPUGraphicsPipeline(dev, &info);
    if (!pipeline) fatal("SDL_CreateGPUGraphicsPipeline (sky body)");
    SDL_ReleaseGPUShader(dev, vs);
    SDL_ReleaseGPUShader(dev, ps);
    return pipeline;
}

/**
 * A blit: the bloom chain's five stages and the tonemap. No vertex buffer at all - the vertex stage
 * builds one fullscreen triangle from the vertex id - and no depth attachment.
 */
SDL_GPUGraphicsPipeline *create_fullscreen_pipeline(SDL_GPUDevice *dev, SDL_GPUTextureFormat format,
                                                   const char *ps_path, const char *ps_entry,
                                                   Uint32 samplers, bool additive) {
    SDL_GPUShader *vs = mesh_shader(dev, "shaders/bloom_vs.dxil", "VSFullscreen",
                                    SDL_GPU_SHADERSTAGE_VERTEX, 0, 0);
    SDL_GPUShader *ps =
        mesh_shader(dev, ps_path, ps_entry, SDL_GPU_SHADERSTAGE_FRAGMENT, 1, samplers);

    SDL_GPURasterizerState rasterizer{};
    rasterizer.fill_mode = SDL_GPU_FILLMODE_FILL;
    rasterizer.cull_mode = SDL_GPU_CULLMODE_NONE;
    rasterizer.front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;

    SDL_GPUDepthStencilState depth{};
    depth.compare_op = SDL_GPU_COMPAREOP_ALWAYS;
    depth.enable_depth_test = false;
    depth.enable_depth_write = false;

    SDL_GPUColorTargetBlendState blend{};
    blend.enable_blend = additive;
    if (additive) {
        // The upsample chain sums the levels: source plus destination, alpha untouched.
        blend.src_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
        blend.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
        blend.color_blend_op = SDL_GPU_BLENDOP_ADD;
        blend.src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
        blend.dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
        blend.alpha_blend_op = SDL_GPU_BLENDOP_ADD;
    }

    SDL_GPUColorTargetDescription target{};
    target.format = format;
    target.blend_state = blend;

    SDL_GPUGraphicsPipelineCreateInfo info{};
    info.vertex_shader = vs;
    info.fragment_shader = ps;
    info.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
    info.rasterizer_state = rasterizer;
    info.multisample_state.sample_count = SDL_GPU_SAMPLECOUNT_1;
    info.depth_stencil_state = depth;
    info.target_info.color_target_descriptions = &target;
    info.target_info.num_color_targets = 1;

    SDL_GPUGraphicsPipeline *pipeline = SDL_CreateGPUGraphicsPipeline(dev, &info);
    if (!pipeline) fatal("SDL_CreateGPUGraphicsPipeline (fullscreen)");
    SDL_ReleaseGPUShader(dev, vs);
    SDL_ReleaseGPUShader(dev, ps);
    return pipeline;
}

SDL_GPUGraphicsPipeline *create_ui_pipeline(SDL_GPUDevice *dev, SDL_GPUTextureFormat format) {
    SDL_GPUShader *vs =
        gpu::make_shader(dev, "shaders/ui_vs.dxil", SDL_GPU_SHADERSTAGE_VERTEX, "VSMain", 1, 0);
    SDL_GPUShader *ps =
        gpu::make_shader(dev, "shaders/ui_ps.dxil", SDL_GPU_SHADERSTAGE_FRAGMENT, "PSMain", 0, 1);

    SDL_GPUVertexBufferDescription buffer{};
    buffer.slot = 0;
    buffer.pitch = sizeof(UIVertex);
    buffer.input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;

    SDL_GPUVertexAttribute attributes[3]{};
    attributes[0] = {0, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2, offsetof(UIVertex, pos)};
    attributes[1] = {1, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2, offsetof(UIVertex, uv)};
    attributes[2] = {2, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4, offsetof(UIVertex, color)};

    SDL_GPUVertexInputState vertex_input{};
    vertex_input.vertex_buffer_descriptions = &buffer;
    vertex_input.num_vertex_buffers = 1;
    vertex_input.vertex_attributes = attributes;
    vertex_input.num_vertex_attributes = 3;

    SDL_GPURasterizerState rasterizer{};
    rasterizer.fill_mode = SDL_GPU_FILLMODE_FILL;
    rasterizer.cull_mode = SDL_GPU_CULLMODE_NONE;
    rasterizer.front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;
    rasterizer.enable_depth_clip = true;

    SDL_GPUColorTargetBlendState blend{};
    blend.src_color_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA;
    blend.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
    blend.color_blend_op = SDL_GPU_BLENDOP_ADD;
    blend.src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
    blend.dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
    blend.alpha_blend_op = SDL_GPU_BLENDOP_ADD;
    blend.enable_blend = true;

    SDL_GPUColorTargetDescription target{};
    target.format = format;
    target.blend_state = blend;

    SDL_GPUGraphicsPipelineCreateInfo info{};
    info.vertex_shader = vs;
    info.fragment_shader = ps;
    info.vertex_input_state = vertex_input;
    info.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
    info.rasterizer_state = rasterizer;
    info.multisample_state.sample_count = SDL_GPU_SAMPLECOUNT_1;
    info.depth_stencil_state.compare_op = SDL_GPU_COMPAREOP_ALWAYS;
    info.target_info.color_target_descriptions = &target;
    info.target_info.num_color_targets = 1;

    SDL_GPUGraphicsPipeline *pipeline = SDL_CreateGPUGraphicsPipeline(dev, &info);
    if (!pipeline) fatal("SDL_CreateGPUGraphicsPipeline (ui)");
    SDL_ReleaseGPUShader(dev, vs);
    SDL_ReleaseGPUShader(dev, ps);
    return pipeline;
}

// ------------------------------------------------------------------ targets

/** Grows a single-sample render target in place. `samples` > 1 builds a resolve-only target. */
SDL_GPUTexture *ensure_target(Renderer &renderer, SDL_GPUTexture *&texture, Uint32 &stored_width,
                              Uint32 &stored_height, Uint32 width, Uint32 height,
                              SDL_GPUTextureFormat format, Uint32 samples) {
    if (texture && stored_width == width && stored_height == height) return texture;
    if (texture) SDL_ReleaseGPUTexture(renderer.device.handle, texture);
    texture = gpu::create_color(renderer.device, width, height, format, sample_count_for(samples));
    stored_width = width;
    stored_height = height;
    return texture;
}

/** The cascade: a depth map that is also sampled, which gpu::create_depth does not build. */
SDL_GPUTexture *create_shadow_map(gpu::Device &device) {
    SDL_GPUTextureCreateInfo info{};
    info.type = SDL_GPU_TEXTURETYPE_2D;
    info.format = SDL_GPU_TEXTUREFORMAT_D32_FLOAT;
    info.usage = SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER;
    info.width = SHADOW_SIZE;
    info.height = SHADOW_SIZE;
    info.layer_count_or_depth = 1;
    info.num_levels = 1;
    info.sample_count = SDL_GPU_SAMPLECOUNT_1;
    SDL_GPUTexture *texture = SDL_CreateGPUTexture(device.handle, &info);
    if (!texture) fatal("SDL_CreateGPUTexture (shadow map)");
    return texture;
}

/**
 * Uploads the frame's UI indices, growing the buffer and its transfer buffer when a frame needs
 * more. The vertex path lives in gpu::DynamicBuffer; this one needs INDEX usage, which that type
 * does not create, so it is the same three calls with a different flag.
 */
void write_ui_indices(Renderer &renderer, SDL_GPUCopyPass *pass, const uint32_t *indices,
                      Uint32 count) {
    if (count == 0) return;
    const Uint32 bytes = count * sizeof(uint32_t);
    if (!renderer.ui_indices || bytes > renderer.ui_index_capacity) {
        const Uint32 grown = bytes + bytes / 2;
        renderer.ui_index_capacity = grown < 4096 ? 4096 : grown;
        if (renderer.ui_indices) SDL_ReleaseGPUBuffer(renderer.device.handle, renderer.ui_indices);
        SDL_GPUBufferCreateInfo info{};
        info.usage = SDL_GPU_BUFFERUSAGE_INDEX;
        info.size = renderer.ui_index_capacity;
        renderer.ui_indices = SDL_CreateGPUBuffer(renderer.device.handle, &info);
        if (!renderer.ui_indices) fatal("SDL_CreateGPUBuffer (ui indices)");
        // The transfer buffer has to grow with it: the copy below memcpys the whole payload.
        if (renderer.ui_index_transfer) {
            SDL_ReleaseGPUTransferBuffer(renderer.device.handle, renderer.ui_index_transfer);
        }
        SDL_GPUTransferBufferCreateInfo transfer_info{};
        transfer_info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
        transfer_info.size = renderer.ui_index_capacity;
        renderer.ui_index_transfer =
            SDL_CreateGPUTransferBuffer(renderer.device.handle, &transfer_info);
        if (!renderer.ui_index_transfer) fatal("SDL_CreateGPUTransferBuffer (ui indices)");
    }
    void *mapped =
        SDL_MapGPUTransferBuffer(renderer.device.handle, renderer.ui_index_transfer, true);
    std::memcpy(mapped, indices, bytes);
    SDL_UnmapGPUTransferBuffer(renderer.device.handle, renderer.ui_index_transfer);
    SDL_GPUTransferBufferLocation source{renderer.ui_index_transfer, 0};
    SDL_GPUBufferRegion destination{renderer.ui_indices, 0, bytes};
    SDL_UploadToGPUBuffer(pass, &source, &destination, true);
}

/** The star's cascade, fitted to the camera's view of the play plane rather than to the whole
 *  12 km depth range: the receivers that matter are the ones within a few hundred metres of the
 *  ship, and a cascade has one resolution to spend. */
glm::mat4 fit_shadow_cascade(const Camera &camera, const glm::vec3 &to_star) {
    const float half = std::max(camera.half_height * camera.aspect, camera.half_height) * 1.25f;
    const glm::vec3 centre(camera.target.x, camera.target.y, 0.0f);
    const glm::vec3 direction = glm::length(to_star) > 1e-4f ? glm::normalize(to_star)
                                                             : glm::vec3(0.0f, 0.0f, 1.0f);
    const glm::vec3 up = std::abs(direction.z) > 0.99f ? glm::vec3(0.0f, 1.0f, 0.0f)
                                                       : glm::vec3(0.0f, 0.0f, 1.0f);
    const glm::mat4 view = glm::lookAt(centre - direction * (2.0f * half), centre, up);
    const glm::mat4 projection =
        glm::ortho(-half, half, -half, half, 0.0f, 4.0f * half);
    return projection * view;
}

/** Which shader a sky body's surface needs, from its angular size in pixels. */
int body_lod(float diameter_pixels) {
    if (diameter_pixels < 4.0f) return 0;
    if (diameter_pixels < 40.0f) return 1;
    return 2;
}

}  // namespace

const PipelineSet &Renderer::pipelines_for(SDL_GPUTextureFormat format, Uint32 requested_samples) {
    const Uint32 wanted = requested_samples >= 4 ? 4 : 1;
    for (const PipelineSet &set : pipelines) {
        if (set.format == format && set.samples == wanted) return set;
    }
    PipelineSet set;
    set.format = format;
    set.samples = wanted;
    set.mesh = create_mesh_pipeline(device.handle, wanted, false, false);
    set.effect = create_mesh_pipeline(device.handle, wanted, true, true);
    set.backdrop = create_mesh_pipeline(device.handle, wanted, true, false);
    set.planet = create_body_pipeline(device.handle, wanted, "shaders/planet_vs.dxil", "VSMain",
                                      "shaders/planet_ps.dxil", "PSMain", false, true, 3);
    set.air = create_body_pipeline(device.handle, wanted, "shaders/planet_vs.dxil", "VSMain",
                                   "shaders/planet_air_ps.dxil", "PSAir", true, false, 0);
    set.star = create_body_pipeline(device.handle, wanted, "shaders/star_vs.dxil", "VSMain",
                                    "shaders/star_ps.dxil", "PSMain", true, false, 1);
    set.shadow = create_shadow_pipeline(device.handle);
    set.bloom_threshold = create_fullscreen_pipeline(
        device.handle, HDR_FORMAT, "shaders/bloom_threshold_ps.dxil", "PSThreshold", 1, false);
    set.bloom_downsample = create_fullscreen_pipeline(
        device.handle, HDR_FORMAT, "shaders/bloom_downsample_ps.dxil", "PSDownsample", 1, false);
    set.bloom_upsample = create_fullscreen_pipeline(
        device.handle, HDR_FORMAT, "shaders/bloom_upsample_ps.dxil", "PSUpsample", 1, true);
    set.bloom_combine = create_fullscreen_pipeline(
        device.handle, HDR_FORMAT, "shaders/bloom_combine_ps.dxil", "PSCombine", 2, false);
    set.tonemap = create_fullscreen_pipeline(device.handle, format, "shaders/tonemap_ps.dxil",
                                             "PSMain", 1, false);
    set.ui = create_ui_pipeline(device.handle, format);
    pipelines.push_back(set);
    return pipelines.back();
}

void ensure_depth(Renderer &renderer, Uint32 width, Uint32 height) {
    if (renderer.depth && renderer.depth_width == width && renderer.depth_height == height &&
        renderer.depth_samples == renderer.samples) {
        return;
    }
    renderer.depth_samples = renderer.samples;
    if (renderer.depth) SDL_ReleaseGPUTexture(renderer.device.handle, renderer.depth);
    renderer.depth = gpu::create_depth(renderer.device, width, height,
                                       sample_count_for(renderer.samples));
    renderer.depth_width = width;
    renderer.depth_height = height;
}

/** Merges every library mesh into one vertex/index pair and records their ranges. */
/** The four rock tiles, loaded once and always first: the asteroid materials name them 0..3. */
void load_rock_tiles(Renderer &renderer) {
    if (!renderer.textures.empty()) return;
    for (const char *name :
         {"rock_silicate", "rock_carbonaceous", "rock_ore", "rock_regolith"}) {
        const render::LoadedTexture loaded =
            render::load_texture(renderer.device, "assets/textures/" + std::string(name) + ".png",
                                 true);
        renderer.textures.push_back(loaded.texture);
    }
}

void upload_mesh_library(Renderer &renderer, const opra::MeshLibrary &library) {
    // A hot reload calls this again: the previous buffers must go or they leak.
    if (renderer.mesh_vertices) SDL_ReleaseGPUBuffer(renderer.device.handle, renderer.mesh_vertices);
    if (renderer.mesh_indices) SDL_ReleaseGPUBuffer(renderer.device.handle, renderer.mesh_indices);
    renderer.mesh_vertices = nullptr;
    renderer.mesh_indices = nullptr;
    Uint32 vertex_total = 0, index_total = 0;
    for (int i = 0; i < library.size(); ++i) {
        vertex_total += static_cast<Uint32>(library.at(i).vertices.size());
        index_total += static_cast<Uint32>(library.at(i).indices.size());
    }
    std::vector<MeshVertex> vertices;
    std::vector<uint32_t> indices;
    vertices.reserve(vertex_total);
    indices.reserve(index_total);

    load_rock_tiles(renderer);
    // Materials are deduplicated across the whole library: a hundred primitives that share one
    // export material share one entry, and the run that binds it is already one draw.
    renderer.materials.clear();
    renderer.meshes.clear();
    for (int i = 0; i < library.size(); ++i) {
        const opra::MeshData &mesh = library.at(i);
        GpuMesh gpu;
        gpu.first_index = static_cast<Uint32>(indices.size());
        gpu.index_count = static_cast<Uint32>(mesh.indices.size());
        gpu.vertex_offset = static_cast<Sint32>(vertices.size());
        // The GLB image blobs are never uploaded (the exporter's maps are procedural detail the
        // draw path does not sample yet), so a model-local texture index names nothing in this
        // table. Only the triplanar rock materials name real entries (the four tiles load_rock_tiles
        // puts at 0..3); everything else samples the white texel, exactly as before the tiles
        // existed - otherwise a hull's slot 0 silently becomes the silicate tile.
        opra::Material material = mesh.material;
        if (!material.triplanar) {
            material.base_color_texture = -1;
            material.metallic_roughness_texture = -1;
            material.normal_texture = -1;
        }
        gpu.material = renderer.materials.add(material);
        vertices.insert(vertices.end(), mesh.vertices.begin(), mesh.vertices.end());
        // Indices stay local to their mesh: the draw call supplies the base vertex. Widening them
        // here as well applies the offset twice and pushes the last meshes out of the buffer.
        for (uint32_t index : mesh.indices) indices.push_back(index);
        renderer.meshes.push_back(gpu);
    }

    renderer.mesh_vertices =
        gpu::upload_static(renderer.device, SDL_GPU_BUFFERUSAGE_VERTEX, vertices.data(),
                      static_cast<Uint32>(vertices.size() * sizeof(MeshVertex)));
    renderer.mesh_indices =
        gpu::upload_static(renderer.device, SDL_GPU_BUFFERUSAGE_INDEX, indices.data(),
                      static_cast<Uint32>(indices.size() * sizeof(uint32_t)));
    SDL_Log("meshes: %d | vertices: %u | triangles: %u | materials: %d | vertex: %u B",
            library.size(), vertex_total, index_total / 3, renderer.materials.size(),
            static_cast<Uint32>(sizeof(MeshVertex)));
}

void destroy_renderer(Renderer &renderer) {
    Renderer &r = renderer;
    r.instances.destroy(r.device);
    r.body_instances.destroy(r.device);
    r.ui_vertices.destroy(r.device);
    if (r.depth) SDL_ReleaseGPUTexture(r.device.handle, r.depth);
    if (r.hdr_color) SDL_ReleaseGPUTexture(r.device.handle, r.hdr_color);
    if (r.hdr_msaa) SDL_ReleaseGPUTexture(r.device.handle, r.hdr_msaa);
    if (r.hdr_lit) SDL_ReleaseGPUTexture(r.device.handle, r.hdr_lit);
    for (SDL_GPUTexture *mip : r.bloom_mips) {
        if (mip) SDL_ReleaseGPUTexture(r.device.handle, mip);
    }
    if (r.shadow_map) SDL_ReleaseGPUTexture(r.device.handle, r.shadow_map);
    if (r.sampler) SDL_ReleaseGPUSampler(r.device.handle, r.sampler);
    if (r.shadow_sampler) SDL_ReleaseGPUSampler(r.device.handle, r.shadow_sampler);
    if (r.mesh_indices) SDL_ReleaseGPUBuffer(r.device.handle, r.mesh_indices);
    if (r.mesh_vertices) SDL_ReleaseGPUBuffer(r.device.handle, r.mesh_vertices);
    if (r.white) SDL_ReleaseGPUTexture(r.device.handle, r.white);
    for (SDL_GPUTexture *texture : r.textures) {
        SDL_ReleaseGPUTexture(r.device.handle, texture);
    }
    r.textures.clear();
    if (r.ui_indices) SDL_ReleaseGPUBuffer(r.device.handle, r.ui_indices);
    if (r.ui_index_transfer) SDL_ReleaseGPUTransferBuffer(r.device.handle, r.ui_index_transfer);
    for (const PipelineSet &set : r.pipelines) {
        SDL_ReleaseGPUGraphicsPipeline(r.device.handle, set.mesh);
        SDL_ReleaseGPUGraphicsPipeline(r.device.handle, set.effect);
        SDL_ReleaseGPUGraphicsPipeline(r.device.handle, set.backdrop);
        SDL_ReleaseGPUGraphicsPipeline(r.device.handle, set.planet);
        SDL_ReleaseGPUGraphicsPipeline(r.device.handle, set.air);
        SDL_ReleaseGPUGraphicsPipeline(r.device.handle, set.star);
        SDL_ReleaseGPUGraphicsPipeline(r.device.handle, set.shadow);
        SDL_ReleaseGPUGraphicsPipeline(r.device.handle, set.bloom_threshold);
        SDL_ReleaseGPUGraphicsPipeline(r.device.handle, set.bloom_downsample);
        SDL_ReleaseGPUGraphicsPipeline(r.device.handle, set.bloom_upsample);
        SDL_ReleaseGPUGraphicsPipeline(r.device.handle, set.bloom_combine);
        SDL_ReleaseGPUGraphicsPipeline(r.device.handle, set.tonemap);
        SDL_ReleaseGPUGraphicsPipeline(r.device.handle, set.ui);
    }
}

void draw_frame(Renderer &renderer, TextEngine &text, SDL_GPUCommandBuffer *cmd,
                SDL_GPUTexture *color, SDL_GPUTextureFormat format, Uint32 width, Uint32 height,
                const Camera &camera, SceneBuilder &scene, UIBatch &ui) {
    ++renderer.frame_index;
    const PipelineSet &set = renderer.pipelines_for(format, renderer.samples);

    std::vector<InstanceRun> &runs = renderer.opaque_runs;
    std::vector<InstanceRun> &effect_runs = renderer.effect_runs;
    std::vector<InstanceRun> &backdrop_runs = renderer.backdrop_runs;
    std::vector<Instance> &instances = renderer.sorted_instances;
    scene.sorted(instances, runs, effect_runs, backdrop_runs,
                 static_cast<int>(renderer.meshes.size()));
    renderer.instance_count = static_cast<Uint32>(instances.size());
    renderer.run_count =
        static_cast<Uint32>(runs.size() + effect_runs.size() + backdrop_runs.size());
    Uint64 triangles = 0;
    const std::vector<InstanceRun> *run_lists[3] = {&runs, &effect_runs, &backdrop_runs};
    for (const std::vector<InstanceRun> *list : run_lists) {
        for (const InstanceRun &run : *list) {
            if (run.mesh < 0 || run.mesh >= static_cast<int>(renderer.meshes.size())) continue;
            triangles += static_cast<Uint64>(renderer.meshes[static_cast<size_t>(run.mesh)].index_count / 3) * run.count;
        }
    }
    renderer.triangle_count = triangles;

    // ------------------------------------------------------------------ the sky bodies
    // Sized once per frame, from the camera: the LOD is the angular size in pixels, and the star's
    // own angular radius is what softens every planet's terminator (plan 4.4).
    const glm::vec3 forward = glm::normalize(camera.target - camera.eye);
    const float pixels_at_unit = (static_cast<float>(height) * 0.5f) / std::tan(CAMERA_FOV_Y * 0.5f);
    float star_pixels = 0.0f;
    for (const SkyBody &body : scene.bodies) {
        if (body.kind != BodyKind::Star) continue;
        const float distance = std::max(glm::length(body.center - camera.eye), body.radius);
        star_pixels = std::max(star_pixels, body.radius * pixels_at_unit / distance);
    }
    const float star_angle = star_pixels > 0.0f ? std::atan(star_pixels / pixels_at_unit) : 0.0f;
    const glm::vec3 star_tint =
        glm::length(scene.light.color) > 0.0f ? scene.light.color : glm::vec3(1.0f);
    const float star_intensity = std::max(scene.light.intensity, 0.0f);

    std::vector<Instance> &sky_instances = renderer.sky_instances;
    std::vector<SkyDraw> &sky_draws = renderer.sky_draws;
    sky_instances.clear();
    sky_draws.clear();
    for (const SkyBody &body : scene.bodies) {
        if (body.mesh < 0 || body.mesh >= static_cast<int>(renderer.meshes.size())) continue;
        const glm::vec3 to_body = body.center - camera.eye;
        const float depth = std::max(glm::dot(to_body, forward), body.radius + 1e-3f);
        const float pixels = body.radius * pixels_at_unit / depth;
        const bool star = body.kind == BodyKind::Star;
        const bool air = !star && body.scale_height > 0.0f && body.atmosphere_top > 0.0f;
        const int lod = body_lod(2.0f * pixels);

        const auto fill = [&](float scale, bool shell) {
            SkyDraw draw;
            draw.instance.pos = glm::vec4(body.center, 1.0f);
            draw.instance.rot = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
            draw.instance.scale = glm::vec4(body.radius * scale, body.radius * scale,
                                            body.radius * scale, 1.0f);
            draw.instance.color = glm::vec4(body.color, 1.0f);
            draw.uniforms.view_proj = view_projection(camera);
            draw.uniforms.center_radius = glm::vec4(body.center, body.radius);
            draw.uniforms.color = glm::vec4(body.color, 1.0f);
            draw.uniforms.terrain =
                glm::vec4(body.terrain_seed, body.terrain_amplitude, static_cast<float>(lod),
                          static_cast<float>(renderer.frame_index) / 60.0f);
            draw.uniforms.atmosphere = glm::vec4(body.scale_height, body.atmosphere_top,
                                                 star_angle, 0.0f);
            draw.uniforms.star_direction =
                glm::vec4(glm::normalize(scene.light.direction_to_star), star_intensity);
            draw.uniforms.star_color = glm::vec4(star_tint, 1.0f);
            draw.uniforms.viewport =
                glm::vec4(static_cast<float>(width), static_cast<float>(height), star_pixels,
                          std::max(pixels, 1e-3f));
            draw.uniforms.eye_position = glm::vec4(camera.eye, 1.0f);
            if (star) {
                draw.maps[0] = renderer.map_texture(body.photosphere_map);
                draw.tile = true;
            } else {
                draw.maps[0] = renderer.map_texture(body.albedo_map);
                draw.maps[1] = renderer.map_texture(body.cloud_map);
                draw.maps[2] = renderer.map_texture(body.night_map);
            }
            draw.uniforms.maps = glm::vec4(draw.maps[0] ? 1.0f : 0.0f, draw.maps[1] ? 1.0f : 0.0f,
                                           draw.maps[2] ? 1.0f : 0.0f, 0.0f);
            draw.mesh = body.mesh;
            draw.shell = shell;
            draw.star = star;
            sky_draws.push_back(draw);
            sky_instances.push_back(draw.instance);
        };

        if (star) {
            // The corona needs geometry past the disc: the sphere is drawn out to CORONA radii.
            fill(3.2f, false);
        } else {
            fill(1.0f, false);
            if (air && lod > 1) fill(1.0f + std::max(body.atmosphere_top, 0.06f), true);
        }
    }

    // ------------------------------------------------------------------ the UI batch
    text.attach(renderer.device);
    std::vector<UIVertex> &vertices = renderer.ui_vertex_scratch;
    std::vector<uint32_t> &indices = renderer.ui_index_scratch;
    vertices.assign(ui.solid.begin(), ui.solid.end());
    indices.resize(vertices.size());
    for (uint32_t i = 0; i < indices.size(); ++i) indices[i] = i;
    renderer.text_runs.clear();
    text.build(ui.texts, vertices, indices, renderer.text_runs);

    // Both dynamic writes go into one copy pass opened before any render pass, so the whole frame
    // costs a single submit and the mesh pass binds buffers that already exist.
    SDL_GPUCopyPass *copy = SDL_BeginGPUCopyPass(cmd);
    renderer.instances.write(renderer.device, copy, instances.data(),
                             static_cast<Uint32>(instances.size() * sizeof(Instance)));
    renderer.body_instances.write(renderer.device, copy, sky_instances.data(),
                                  static_cast<Uint32>(sky_instances.size() * sizeof(Instance)));
    renderer.ui_vertices.write(renderer.device, copy, vertices.data(),
                               static_cast<Uint32>(vertices.size() * sizeof(UIVertex)));
    write_ui_indices(renderer, copy, indices.data(), static_cast<Uint32>(indices.size()));
    SDL_EndGPUCopyPass(copy);

    if (!renderer.white) {
        // The UI shader always samples, the solids take a texel of their own, and a material with no
        // texture of its own samples this one with its flag clear.
        const Uint8 texel[4] = {255, 255, 255, 255};
        renderer.white = gpu::upload_texture(renderer.device, 1, 1, texel);
    }
    if (!renderer.shadow_sampler) {
        SDL_GPUSamplerCreateInfo info{};
        info.min_filter = SDL_GPU_FILTER_NEAREST;
        info.mag_filter = SDL_GPU_FILTER_NEAREST;
        info.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST;
        info.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
        info.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
        info.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
        renderer.shadow_sampler = SDL_CreateGPUSampler(renderer.device.handle, &info);
        if (!renderer.shadow_sampler) fatal("SDL_CreateGPUSampler (shadow)");
    }
    if (!renderer.map_sampler_equirect || !renderer.map_sampler_tile) {
        // An equirectangular map wraps in longitude and clamps at the poles (a map's top and
        // bottom edge are the poles, not a tile); a tile wraps in both. Linear with mips, since
        // the load_texture chains them.
        SDL_GPUSamplerCreateInfo info{};
        info.min_filter = SDL_GPU_FILTER_LINEAR;
        info.mag_filter = SDL_GPU_FILTER_LINEAR;
        info.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_LINEAR;
        info.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_REPEAT;
        info.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
        info.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_REPEAT;
        renderer.map_sampler_equirect = SDL_CreateGPUSampler(renderer.device.handle, &info);
        info.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_REPEAT;
        renderer.map_sampler_tile = SDL_CreateGPUSampler(renderer.device.handle, &info);
        if (!renderer.map_sampler_equirect || !renderer.map_sampler_tile) {
            fatal("SDL_CreateGPUSampler (body maps)");
        }
    }

    // ------------------------------------------------------------------ the star's cascade
    const glm::mat4 shadow_matrix = fit_shadow_cascade(camera, scene.light.direction_to_star);
    renderer.shadow_view_proj = shadow_matrix;
    const float cascade_half =
        std::max(camera.half_height * camera.aspect, camera.half_height) * 1.25f;
    const bool casting = !runs.empty() && !renderer.meshes.empty();
    if (casting) {
        if (!renderer.shadow_map) renderer.shadow_map = create_shadow_map(renderer.device);
        SDL_GPUDepthStencilTargetInfo depth_target{};
        depth_target.texture = renderer.shadow_map;
        depth_target.clear_depth = 1.0f;
        depth_target.load_op = SDL_GPU_LOADOP_CLEAR;
        depth_target.store_op = SDL_GPU_STOREOP_STORE;
        SDL_PushGPUVertexUniformData(cmd, 0, &shadow_matrix, sizeof shadow_matrix);

        SDL_GPURenderPass *pass = SDL_BeginGPURenderPass(cmd, nullptr, 0, &depth_target);
        SDL_BindGPUGraphicsPipeline(pass, set.shadow);
        SDL_GPUBufferBinding vertex_bindings[2]{};
        vertex_bindings[0] = {renderer.mesh_vertices, 0};
        vertex_bindings[1] = {renderer.instances.buffer, 0};
        SDL_BindGPUVertexBuffers(pass, 0, vertex_bindings, 2);
        SDL_GPUBufferBinding index_binding{renderer.mesh_indices, 0};
        SDL_BindGPUIndexBuffer(pass, &index_binding, SDL_GPU_INDEXELEMENTSIZE_32BIT);
        for (const InstanceRun &run : runs) {
            const GpuMesh &mesh = renderer.meshes[static_cast<size_t>(run.mesh)];
            SDL_DrawGPUIndexedPrimitives(pass, mesh.index_count, run.count, mesh.first_index,
                                         mesh.vertex_offset, run.first_instance);
        }
        SDL_EndGPURenderPass(pass);
    }

    // ------------------------------------------------------------------ the scene
    ensure_target(renderer, renderer.hdr_color, renderer.hdr_width, renderer.hdr_height, width,
                  height, HDR_FORMAT, 1);
    SDL_GPUTexture *scene_target = renderer.hdr_color;
    if (renderer.samples > 1) {
        scene_target = ensure_target(renderer, renderer.hdr_msaa, renderer.msaa_width,
                                     renderer.msaa_height, width, height, HDR_FORMAT,
                                     renderer.samples);
    }

    MeshUniforms mesh_uniforms{};
    mesh_uniforms.view_proj = view_projection(camera);
    mesh_uniforms.light_dir =
        glm::vec4(glm::normalize(scene.light.direction_to_star), 0.34f);
    mesh_uniforms.tint = glm::vec4(1.0f, 0.99f, 0.96f, 1.0f);
    mesh_uniforms.view_dir = glm::vec4(glm::normalize(camera.target - camera.eye), 0.35f);
    // Fill light from the opposite side, dim: it keeps the shadowed faces readable, not lit.
    const glm::vec3 key = glm::normalize(scene.light.direction_to_star);
    mesh_uniforms.fill_dir =
        glm::vec4(glm::normalize(glm::vec3(-key.x, -key.y, 0.5f)), 0.25f);
    mesh_uniforms.light_view_proj = shadow_matrix;
    mesh_uniforms.light_color =
        glm::vec4(scene.light.color, std::max(scene.light.intensity, 0.0f));
    mesh_uniforms.shadow_params =
        glm::vec4(2.0f * cascade_half / static_cast<float>(SHADOW_SIZE) * 1.5f, 0.0006f,
                  1.0f / static_cast<float>(SHADOW_SIZE), casting ? 1.0f : 0.0f);

    SDL_GPUColorTargetInfo color_target{};
    color_target.texture = scene_target;
    // The void, in linear light: the old #070d15 after the tonemap sits where #070d15 did.
    color_target.clear_color = SDL_FColor{0.006f, 0.012f, 0.022f, 1.0f};
    color_target.load_op = SDL_GPU_LOADOP_CLEAR;
    color_target.store_op = SDL_GPU_STOREOP_STORE;
    // With MSAA the scene renders into the multisample texture and resolves into the single-sample
    // HDR image as that pass ends.
    const bool resolving = scene_target != renderer.hdr_color;
    if (resolving) {
        color_target.store_op = SDL_GPU_STOREOP_RESOLVE_AND_STORE;
        color_target.resolve_texture = renderer.hdr_color;
    }

    SDL_GPUDepthStencilTargetInfo depth_target{};
    depth_target.texture = renderer.depth;
    // Reversed-Z: the far plane is 0, so that is what an empty depth buffer holds.
    depth_target.clear_depth = 0.0f;
    depth_target.load_op = SDL_GPU_LOADOP_CLEAR;
    depth_target.store_op = SDL_GPU_STOREOP_STORE;

    SDL_GPURenderPass *pass = SDL_BeginGPURenderPass(cmd, &color_target, 1, &depth_target);
    SDL_PushGPUVertexUniformData(cmd, 0, &mesh_uniforms, sizeof mesh_uniforms);
    // The pixel stage reads the same block: lighting is decided per pixel.
    SDL_PushGPUFragmentUniformData(cmd, 0, &mesh_uniforms, sizeof mesh_uniforms);

    SDL_GPUBufferBinding vertex_bindings[2]{};
    vertex_bindings[0] = {renderer.mesh_vertices, 0};
    vertex_bindings[1] = {renderer.instances.buffer, 0};
    SDL_BindGPUVertexBuffers(pass, 0, vertex_bindings, 2);
    SDL_GPUBufferBinding index_binding{renderer.mesh_indices, 0};
    SDL_BindGPUIndexBuffer(pass, &index_binding, SDL_GPU_INDEXELEMENTSIZE_32BIT);

    // The sky bodies first: the planet surfaces write depth, the star and the air shells add to the
    // frame around them.
    if (!sky_draws.empty()) {
        SDL_GPUBufferBinding sky_bindings[2]{};
        sky_bindings[0] = {renderer.mesh_vertices, 0};
        sky_bindings[1] = {renderer.body_instances.buffer, 0};
        SDL_BindGPUVertexBuffers(pass, 0, sky_bindings, 2);
        const auto draw_sky = [&](SDL_GPUGraphicsPipeline *pipeline, bool shell, bool star,
                                  Uint32 samplers) {
            int drawn = 0;
            for (size_t i = 0; i < sky_draws.size(); ++i) {
                if (sky_draws[i].shell != shell || sky_draws[i].star != star) continue;
                if (drawn == 0) SDL_BindGPUGraphicsPipeline(pass, pipeline);
                ++drawn;
                // A body samples the maps its system file named, and the white texel for every
                // slot it did not, so the shader's flag decides and the sampler is never a hole.
                if (samplers > 0) {
                    SDL_GPUTextureSamplerBinding bindings[3]{};
                    for (Uint32 slot = 0; slot < samplers; ++slot) {
                        bindings[slot] = {
                            sky_draws[i].maps[slot] ? sky_draws[i].maps[slot] : renderer.white,
                            sky_draws[i].tile ? renderer.map_sampler_tile
                                              : renderer.map_sampler_equirect};
                    }
                    SDL_BindGPUFragmentSamplers(pass, 0, bindings, samplers);
                }
                SDL_PushGPUVertexUniformData(cmd, 0, &sky_draws[i].uniforms,
                                             sizeof(BodyUniforms));
                SDL_PushGPUFragmentUniformData(cmd, 0, &sky_draws[i].uniforms,
                                               sizeof(BodyUniforms));
                const GpuMesh &mesh = renderer.meshes[static_cast<size_t>(sky_draws[i].mesh)];
                SDL_DrawGPUIndexedPrimitives(pass, mesh.index_count, 1, mesh.first_index,
                                             mesh.vertex_offset, static_cast<Uint32>(i));
            }
        };
        draw_sky(set.planet, false, false, 3);
        draw_sky(set.air, true, false, 0);
        draw_sky(set.star, false, true, 1);
        SDL_BindGPUVertexBuffers(pass, 0, vertex_bindings, 2);
    }

    SDL_BindGPUGraphicsPipeline(pass, set.mesh);
    const auto draw_runs = [&](const std::vector<InstanceRun> &list, bool per_material) {
        for (const InstanceRun &run : list) {
            if (run.count == 0 || run.mesh < 0 ||
                run.mesh >= static_cast<int>(renderer.meshes.size())) {
                continue;
            }
            const GpuMesh &mesh = renderer.meshes[static_cast<size_t>(run.mesh)];
            if (per_material) {
                const Material &material = renderer.materials.at(mesh.material);
                // A slot only counts when the texture was actually uploaded: sampling the white
                // texel for a map that is not there would read roughness 1 and metallic 1 out of
                // nothing.
                const auto bound = [&renderer](int index) {
                    return index >= 0 && index < static_cast<int>(renderer.textures.size()) ? index
                                                                                            : -1;
                };
                const int base_texture = bound(material.base_color_texture);
                const int mr_texture = bound(material.metallic_roughness_texture);
                const int normal_texture = bound(material.normal_texture);
                MaterialUniforms uniforms{};
                uniforms.base_color = material.base_color_factor;
                uniforms.metallic_roughness = glm::vec4(
                    material.metallic_factor, material.roughness_factor,
                    material.triplanar ? 1.0f : 0.0f, 0.0f);
                uniforms.texture_flags =
                    glm::vec4(base_texture >= 0 ? 1.0f : 0.0f, mr_texture >= 0 ? 1.0f : 0.0f,
                              normal_texture >= 0 ? 1.0f : 0.0f, material.unlit ? 1.0f : 0.0f);
                SDL_PushGPUFragmentUniformData(cmd, 1, &uniforms, sizeof uniforms);
                SDL_GPUTextureSamplerBinding bindings[4]{};
                const int slots[3] = {base_texture, mr_texture, normal_texture};
                for (int slot = 0; slot < 3; ++slot) {
                    bindings[slot] = {slots[slot] >= 0
                                          ? renderer.textures[static_cast<size_t>(slots[slot])]
                                          : renderer.white,
                                      renderer.sampler};
                }
                bindings[3] = {renderer.shadow_map ? renderer.shadow_map : renderer.white,
                               renderer.shadow_sampler};
                SDL_BindGPUFragmentSamplers(pass, 0, bindings, 4);
            }
            SDL_DrawGPUIndexedPrimitives(pass, mesh.index_count, run.count, mesh.first_index,
                                         mesh.vertex_offset, run.first_instance);
        }
    };
    draw_runs(runs, true);
    // Effects additively, without writing depth: flames glow through each other.
    if (!effect_runs.empty()) {
        SDL_BindGPUGraphicsPipeline(pass, set.effect);
        draw_runs(effect_runs, false);
    }
    // The sky last, also additive and depth-tested: it is light, so it must not occlude the
    // stars behind it or paint a black rim over them.
    if (!backdrop_runs.empty()) {
        SDL_BindGPUGraphicsPipeline(pass, set.backdrop);
        draw_runs(backdrop_runs, false);
    }
    SDL_EndGPURenderPass(pass);

    // ------------------------------------------------------------------ the bloom chain
    // Five levels, each half the last: the threshold lands in level 0, the downsample chain fills
    // the rest, and the upsample chain adds each level back into the one above it.
    for (int mip = 0; mip < BLOOM_MIPS; ++mip) {
        const Uint32 divisor = 1u << (mip + 1);
        const Uint32 mip_width = std::max(width / divisor, 1u);
        const Uint32 mip_height = std::max(height / divisor, 1u);
        ensure_target(renderer, renderer.bloom_mips[mip], renderer.bloom_width[mip],
                      renderer.bloom_height[mip], mip_width, mip_height, HDR_FORMAT, 1);
    }
    ensure_target(renderer, renderer.hdr_lit, renderer.lit_width, renderer.lit_height, width,
                  height, HDR_FORMAT, 1);

    const auto blit = [&](SDL_GPUGraphicsPipeline *pipeline, SDL_GPUTexture *target,
                          const SDL_GPUTexture *source, const SDL_GPUTexture *secondary,
                          const BloomUniforms &uniforms, bool load) {
        SDL_PushGPUFragmentUniformData(cmd, 0, &uniforms, sizeof uniforms);
        SDL_GPUColorTargetInfo target_info{};
        target_info.texture = target;
        target_info.load_op = load ? SDL_GPU_LOADOP_LOAD : SDL_GPU_LOADOP_CLEAR;
        target_info.store_op = SDL_GPU_STOREOP_STORE;
        target_info.clear_color = SDL_FColor{0.0f, 0.0f, 0.0f, 1.0f};
        SDL_GPURenderPass *blit_pass = SDL_BeginGPURenderPass(cmd, &target_info, 1, nullptr);
        SDL_BindGPUGraphicsPipeline(blit_pass, pipeline);
        SDL_GPUTextureSamplerBinding bindings[2]{};
        bindings[0] = {const_cast<SDL_GPUTexture *>(source), renderer.sampler};
        bindings[1] = {const_cast<SDL_GPUTexture *>(secondary ? secondary : source),
                       renderer.sampler};
        SDL_BindGPUFragmentSamplers(blit_pass, 0, bindings, secondary ? 2 : 1);
        SDL_DrawGPUPrimitives(blit_pass, 3, 1, 0, 0);
        SDL_EndGPURenderPass(blit_pass);
    };

    BloomUniforms bloom{};
    bloom.params = glm::vec4(1.0f, 0.5f, 1.0f, 0.0f);
    bloom.texel = glm::vec4(1.0f / static_cast<float>(width), 1.0f / static_cast<float>(height),
                            1.0f / static_cast<float>(renderer.bloom_width[0]),
                            1.0f / static_cast<float>(renderer.bloom_height[0]));
    blit(set.bloom_threshold, renderer.bloom_mips[0], renderer.hdr_color, nullptr, bloom, false);

    for (int mip = 1; mip < BLOOM_MIPS; ++mip) {
        BloomUniforms down{};
        down.params = bloom.params;
        down.texel = glm::vec4(1.0f / static_cast<float>(renderer.bloom_width[mip - 1]),
                               1.0f / static_cast<float>(renderer.bloom_height[mip - 1]),
                               1.0f / static_cast<float>(renderer.bloom_width[mip]),
                               1.0f / static_cast<float>(renderer.bloom_height[mip]));
        blit(set.bloom_downsample, renderer.bloom_mips[mip], renderer.bloom_mips[mip - 1], nullptr,
             down, false);
    }
    for (int mip = BLOOM_MIPS - 1; mip > 0; --mip) {
        BloomUniforms up{};
        // Additive: each level is added into the one above it, and the strength keeps the sum from
        // running away as five haloes stack.
        up.params = glm::vec4(bloom.params.x, bloom.params.y, 0.75f, 0.0f);
        up.texel = glm::vec4(1.0f / static_cast<float>(renderer.bloom_width[mip]),
                             1.0f / static_cast<float>(renderer.bloom_height[mip]),
                             1.0f / static_cast<float>(renderer.bloom_width[mip - 1]),
                             1.0f / static_cast<float>(renderer.bloom_height[mip - 1]));
        blit(set.bloom_upsample, renderer.bloom_mips[mip - 1], renderer.bloom_mips[mip], nullptr,
             up, true);
    }
    BloomUniforms combine{};
    combine.params = glm::vec4(bloom.params.x, bloom.params.y, 0.65f, 0.0f);
    combine.texel = bloom.texel;
    blit(set.bloom_combine, renderer.hdr_lit, renderer.hdr_color, renderer.bloom_mips[0], combine,
         false);

    // ------------------------------------------------------------------ tonemap, then the HUD
    {
        GradeUniforms grade{};
        grade.params = glm::vec4(1.0f, 0.0f, 1.05f, 1.06f);
        SDL_PushGPUFragmentUniformData(cmd, 0, &grade, sizeof grade);
        SDL_GPUColorTargetInfo target_info{};
        target_info.texture = color;
        target_info.load_op = SDL_GPU_LOADOP_DONT_CARE;
        target_info.store_op = SDL_GPU_STOREOP_STORE;
        SDL_GPURenderPass *grade_pass = SDL_BeginGPURenderPass(cmd, &target_info, 1, nullptr);
        SDL_BindGPUGraphicsPipeline(grade_pass, set.tonemap);
        SDL_GPUTextureSamplerBinding binding{renderer.hdr_lit, renderer.sampler};
        SDL_BindGPUFragmentSamplers(grade_pass, 0, &binding, 1);
        SDL_DrawGPUPrimitives(grade_pass, 3, 1, 0, 0);
        SDL_EndGPURenderPass(grade_pass);
    }

    if (vertices.empty()) return;  // nothing to mark up: the frame is the scene alone

    UIUniforms ui_uniforms{};
    ui_uniforms.screen = glm::vec2(static_cast<float>(width), static_cast<float>(height));
    SDL_PushGPUVertexUniformData(cmd, 0, &ui_uniforms, sizeof ui_uniforms);

    // The HUD draws into the tonemapped image: its palette is authored in display space, and its
    // pipeline is single-sampled because its glyphs carry their own coverage.
    SDL_GPUColorTargetInfo hud_target{};
    hud_target.texture = color;
    hud_target.load_op = SDL_GPU_LOADOP_LOAD;
    hud_target.store_op = SDL_GPU_STOREOP_STORE;

    SDL_GPURenderPass *hud_pass = SDL_BeginGPURenderPass(cmd, &hud_target, 1, nullptr);
    SDL_BindGPUGraphicsPipeline(hud_pass, set.ui);
    SDL_GPUBufferBinding ui_binding{renderer.ui_vertices.buffer, 0};
    SDL_BindGPUVertexBuffers(hud_pass, 0, &ui_binding, 1);
    SDL_GPUBufferBinding ui_index_binding{renderer.ui_indices, 0};
    SDL_BindGPUIndexBuffer(hud_pass, &ui_index_binding, SDL_GPU_INDEXELEMENTSIZE_32BIT);
    // The solids first, against the white texel, then one indexed draw per atlas the frame's text
    // needs: the index ranges were laid out by TextEngine::build in exactly that order.
    SDL_GPUTextureSamplerBinding solid_binding{renderer.white, renderer.sampler};
    SDL_BindGPUFragmentSamplers(hud_pass, 0, &solid_binding, 1);
    if (!ui.solid.empty()) {
        SDL_DrawGPUIndexedPrimitives(hud_pass, static_cast<Uint32>(ui.solid.size()), 1, 0, 0, 0);
    }
    for (const TextRun &run : renderer.text_runs) {
        SDL_GPUTextureSamplerBinding binding{run.texture, renderer.sampler};
        SDL_BindGPUFragmentSamplers(hud_pass, 0, &binding, 1);
        SDL_DrawGPUIndexedPrimitives(hud_pass, run.index_count, 1, run.first_index, 0, 0);
    }
    SDL_EndGPURenderPass(hud_pass);
}

/**
 * Resolves a body's map name against assets/textures/manifest.json, which the repair pass wrote
 * with the maps themselves. The whole manifest loads the first time any name is asked for; an
 * unknown name is a null texture, so the body keeps its procedural shading (plan-04 s3.4).
 */
SDL_GPUTexture *Renderer::map_texture(const std::string &name) {
    if (name.empty()) return nullptr;
    if (auto found = body_maps.find(name); found != body_maps.end()) return found->second;
    if (!body_manifest_read) {
        body_manifest_read = true;
        std::ifstream manifest("assets/textures/manifest.json");
        if (!manifest) SDL_Log("map_texture: the manifest does not open (cwd below the repo?)");
        if (manifest) {
            json entries;
            try {
                manifest >> entries;
            } catch (const std::exception &error) {
                SDL_Log("assets/textures/manifest.json: %s", error.what());
            }
            for (auto entry = entries.begin(); entry != entries.end(); ++entry) {
                if (body_maps.count(entry.key())) continue;
                const std::string path = "assets/textures/" + entry.key() + ".png";
                const bool srgb = entry.value().value("srgb", true);
                const render::LoadedTexture loaded = render::load_texture(device, path, srgb);
                body_maps[entry.key()] = loaded.texture;
                body_map_textures.push_back(loaded.texture);
            }
        }
    }
    if (auto found = body_maps.find(name); found != body_maps.end()) return found->second;
    SDL_Log("map_texture: %s is not in the manifest", name.c_str());
    return nullptr;
}

}  // namespace opra
