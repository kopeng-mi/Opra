// The GPU layer: the device, growing dynamic buffers, static uploads and render targets.
// This is the only place outside render/ that names SDL_GPU.
#pragma once

#include <string>

#include <SDL3/SDL.h>

namespace opra::gpu {

/** The context everything else is created from, plus the swapchain format to build against. */
struct Device {
    SDL_GPUDevice *handle = nullptr;
    SDL_Window *window = nullptr;
    SDL_GPUTextureFormat swap_format = SDL_GPU_TEXTUREFORMAT_INVALID;
};

/** Creates the device and claims the window. Fatal on failure: there is no software path. */
Device create_device(SDL_Window *window, bool debug);

/** Releases the device and gives the window back. */
void destroy_device(Device &device);

/**
 * A vertex/instance buffer that grows by 1.5x and keeps one transfer buffer for its lifetime, so a
 * frame never allocates. `write` stages data and records the copy on the caller's command buffer:
 * with one copy pass per frame, the whole frame costs a single submit.
 */
struct DynamicBuffer {
    SDL_GPUBuffer *buffer = nullptr;
    SDL_GPUTransferBuffer *transfer = nullptr;
    Uint32 capacity = 0;
    Uint32 size = 0;

    void ensure(Device &device, Uint32 bytes);
    /** Records the copy into the caller's open copy pass, so a frame costs one submit. */
    void write(Device &device, SDL_GPUCopyPass *pass, const void *data, Uint32 bytes);
    void destroy(Device &device);
};

/** Uploads immutable data (the mesh library) and submits it once, at startup. */
SDL_GPUBuffer *upload_static(Device &device, SDL_GPUBufferUsageFlags usage, const void *data,
                             Uint32 bytes);

/** Uploads RGBA8 pixels as a sampled texture and submits it once. */
SDL_GPUTexture *upload_texture(Device &device, Uint32 width, Uint32 height, const void *rgba_pixels);

/** Replaces the contents of a texture made by upload_texture (the glyph atlas grows). */
void update_texture(Device &device, SDL_GPUTexture *texture, Uint32 width, Uint32 height,
                    const void *rgba_pixels);

SDL_GPUShader *make_shader(SDL_GPUDevice *handle, const std::string &path, SDL_GPUShaderStage stage,
                           const char *entrypoint, Uint32 uniform_buffers, Uint32 samplers);

SDL_GPUTexture *create_depth(Device &device, Uint32 width, Uint32 height,
                             SDL_GPUSampleCount samples);

SDL_GPUTexture *create_color(Device &device, Uint32 width, Uint32 height,
                             SDL_GPUTextureFormat format, SDL_GPUSampleCount samples);

/**
 * Submits a frame's command buffer. Every submit in the program goes through here, so the frame
 * budget stays measurable: one per frame, plus static uploads at startup.
 */
bool submit(SDL_GPUCommandBuffer *cmd);

/** Number of command buffers submitted since startup; the frame budget is one. */
int submit_count();
void reset_submit_count();

}  // namespace opra::gpu
