#include "gpu/gpu.h"

#include <algorithm>
#include <cstring>

#include "core/file.h"
#include "core/log.h"

namespace opra::gpu {
namespace {

int g_submits = 0;

SDL_GPUTransferBuffer *create_transfer(SDL_GPUDevice *device, SDL_GPUTransferBufferUsage usage,
                                       Uint32 size) {
    SDL_GPUTransferBufferCreateInfo info{};
    info.usage = usage;
    info.size = size;
    SDL_GPUTransferBuffer *transfer = SDL_CreateGPUTransferBuffer(device, &info);
    if (!transfer) fatal("SDL_CreateGPUTransferBuffer");
    return transfer;
}

}  // namespace

bool submit(SDL_GPUCommandBuffer *cmd) {
    ++g_submits;
    return SDL_SubmitGPUCommandBuffer(cmd);
}

int submit_count() { return g_submits; }
void reset_submit_count() { g_submits = 0; }

Device create_device(SDL_Window *window, bool debug) {
    Device device;
    device.window = window;
    device.handle = SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_DXIL | SDL_GPU_SHADERFORMAT_SPIRV,
                                        debug, nullptr);
    if (!device.handle) fatal("SDL_CreateGPUDevice");
    if (!SDL_ClaimWindowForGPUDevice(device.handle, window)) fatal("SDL_ClaimWindowForGPUDevice");
    device.swap_format = SDL_GetGPUSwapchainTextureFormat(device.handle, window);
    return device;
}

void destroy_device(Device &device) {
    if (!device.handle) return;
    SDL_WaitForGPUIdle(device.handle);
    SDL_ReleaseWindowFromGPUDevice(device.handle, device.window);
    SDL_DestroyGPUDevice(device.handle);
    device.handle = nullptr;
}

void DynamicBuffer::ensure(Device &device, Uint32 bytes) {
    size = bytes;
    if (buffer && bytes <= capacity) return;
    capacity = std::max<Uint32>(bytes + bytes / 2, 4096);
    if (buffer) SDL_ReleaseGPUBuffer(device.handle, buffer);
    SDL_GPUBufferCreateInfo info{};
    info.usage = SDL_GPU_BUFFERUSAGE_VERTEX;
    info.size = capacity;
    buffer = SDL_CreateGPUBuffer(device.handle, &info);
    if (!buffer) fatal("SDL_CreateGPUBuffer");
    // The transfer buffer has to grow with the GPU buffer: write() memcpys the whole payload into
    // it, so a stale smaller mapping is a heap overflow the moment a frame grows the buffer.
    if (transfer) SDL_ReleaseGPUTransferBuffer(device.handle, transfer);
    transfer = create_transfer(device.handle, SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD, capacity);
}

void DynamicBuffer::write(Device &device, SDL_GPUCopyPass *pass, const void *data, Uint32 bytes) {
    if (bytes == 0) return;
    ensure(device, bytes);
    // cycle = true: a write never waits on the frame still in flight.
    void *mapped = SDL_MapGPUTransferBuffer(device.handle, transfer, true);
    std::memcpy(mapped, data, bytes);
    SDL_UnmapGPUTransferBuffer(device.handle, transfer);
    SDL_GPUTransferBufferLocation source{transfer, 0};
    SDL_GPUBufferRegion destination{buffer, 0, bytes};
    SDL_UploadToGPUBuffer(pass, &source, &destination, true);
}

void DynamicBuffer::destroy(Device &device) {
    if (buffer) SDL_ReleaseGPUBuffer(device.handle, buffer);
    if (transfer) SDL_ReleaseGPUTransferBuffer(device.handle, transfer);
    buffer = nullptr;
    transfer = nullptr;
    capacity = 0;
}

SDL_GPUBuffer *upload_static(Device &device, SDL_GPUBufferUsageFlags usage, const void *data,
                             Uint32 bytes) {
    SDL_GPUBufferCreateInfo buffer_info{};
    buffer_info.usage = usage;
    buffer_info.size = bytes;
    SDL_GPUBuffer *buffer = SDL_CreateGPUBuffer(device.handle, &buffer_info);
    if (!buffer) fatal("SDL_CreateGPUBuffer");

    SDL_GPUTransferBuffer *transfer =
        create_transfer(device.handle, SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD, bytes);
    void *mapped = SDL_MapGPUTransferBuffer(device.handle, transfer, false);
    std::memcpy(mapped, data, bytes);
    SDL_UnmapGPUTransferBuffer(device.handle, transfer);

    SDL_GPUCommandBuffer *cmd = SDL_AcquireGPUCommandBuffer(device.handle);
    SDL_GPUCopyPass *copy = SDL_BeginGPUCopyPass(cmd);
    SDL_GPUTransferBufferLocation source{transfer, 0};
    SDL_GPUBufferRegion destination{buffer, 0, bytes};
    SDL_UploadToGPUBuffer(copy, &source, &destination, false);
    SDL_EndGPUCopyPass(copy);
    if (!submit(cmd)) fatal("SDL_SubmitGPUCommandBuffer (static upload)");
    SDL_ReleaseGPUTransferBuffer(device.handle, transfer);
    return buffer;
}

SDL_GPUTexture *upload_texture(Device &device, Uint32 width, Uint32 height,
                               const void *rgba_pixels) {
    SDL_GPUTextureCreateInfo info{};
    info.type = SDL_GPU_TEXTURETYPE_2D;
    info.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
    info.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;
    info.width = width;
    info.height = height;
    info.layer_count_or_depth = 1;
    info.num_levels = 1;
    SDL_GPUTexture *texture = SDL_CreateGPUTexture(device.handle, &info);
    if (!texture) fatal("SDL_CreateGPUTexture (texture)");

    const Uint32 bytes = width * height * 4;
    SDL_GPUTransferBuffer *transfer =
        create_transfer(device.handle, SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD, bytes);
    void *mapped = SDL_MapGPUTransferBuffer(device.handle, transfer, false);
    std::memcpy(mapped, rgba_pixels, bytes);
    SDL_UnmapGPUTransferBuffer(device.handle, transfer);

    SDL_GPUCommandBuffer *cmd = SDL_AcquireGPUCommandBuffer(device.handle);
    SDL_GPUCopyPass *copy = SDL_BeginGPUCopyPass(cmd);
    SDL_GPUTextureTransferInfo source{};
    source.transfer_buffer = transfer;
    source.offset = 0;
    source.pixels_per_row = width;
    source.rows_per_layer = height;
    SDL_GPUTextureRegion destination{};
    destination.texture = texture;
    destination.w = width;
    destination.h = height;
    destination.d = 1;
    SDL_UploadToGPUTexture(copy, &source, &destination, false);
    SDL_EndGPUCopyPass(copy);
    submit(cmd);
    SDL_ReleaseGPUTransferBuffer(device.handle, transfer);
    return texture;
}

void update_texture(Device &device, SDL_GPUTexture *texture, Uint32 width, Uint32 height,
                    const void *rgba_pixels) {
    const Uint32 bytes = width * height * 4;
    SDL_GPUTransferBuffer *transfer =
        create_transfer(device.handle, SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD, bytes);
    void *mapped = SDL_MapGPUTransferBuffer(device.handle, transfer, false);
    std::memcpy(mapped, rgba_pixels, bytes);
    SDL_UnmapGPUTransferBuffer(device.handle, transfer);

    SDL_GPUCommandBuffer *cmd = SDL_AcquireGPUCommandBuffer(device.handle);
    SDL_GPUCopyPass *copy = SDL_BeginGPUCopyPass(cmd);
    SDL_GPUTextureTransferInfo source{};
    source.transfer_buffer = transfer;
    source.pixels_per_row = width;
    source.rows_per_layer = height;
    SDL_GPUTextureRegion destination{};
    destination.texture = texture;
    destination.w = width;
    destination.h = height;
    destination.d = 1;
    // Cycle: the atlas is bound by frames still in flight. The whole atlas is uploaded here,
    // so the fresh allocation is fully defined.
    SDL_UploadToGPUTexture(copy, &source, &destination, true);
    SDL_EndGPUCopyPass(copy);
    submit(cmd);
    SDL_ReleaseGPUTransferBuffer(device.handle, transfer);
}

SDL_GPUShader *make_shader(SDL_GPUDevice *handle, const std::string &path, SDL_GPUShaderStage stage,
                           const char *entrypoint, Uint32 uniform_buffers, Uint32 samplers) {
    std::vector<Uint8> code = read_file(asset_path(path));
    SDL_GPUShaderCreateInfo info{};
    info.code_size = code.size();
    info.code = code.data();
    info.entrypoint = entrypoint;
    info.format = SDL_GPU_SHADERFORMAT_DXIL;
    info.stage = stage;
    info.num_uniform_buffers = uniform_buffers;
    info.num_samplers = samplers;
    info.num_storage_textures = 0;
    info.num_storage_buffers = 0;
    SDL_GPUShader *shader = SDL_CreateGPUShader(handle, &info);
    if (!shader) fatal(("SDL_CreateGPUShader " + path).c_str());
    return shader;
}

SDL_GPUTexture *create_depth(Device &device, Uint32 width, Uint32 height,
                             SDL_GPUSampleCount samples) {
    SDL_GPUTextureCreateInfo info{};
    info.type = SDL_GPU_TEXTURETYPE_2D;
    info.format = SDL_GPU_TEXTUREFORMAT_D32_FLOAT;
    info.usage = SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET;
    info.width = width;
    info.height = height;
    info.layer_count_or_depth = 1;
    info.num_levels = 1;
    info.sample_count = samples;
    SDL_GPUTexture *texture = SDL_CreateGPUTexture(device.handle, &info);
    if (!texture) fatal("SDL_CreateGPUTexture (depth)");
    return texture;
}

SDL_GPUTexture *create_color(Device &device, Uint32 width, Uint32 height,
                             SDL_GPUTextureFormat format, SDL_GPUSampleCount samples) {
    SDL_GPUTextureCreateInfo info{};
    info.type = SDL_GPU_TEXTURETYPE_2D;
    info.format = format;
    // A multisampled target is never sampled - it is resolved into the swapchain - and asking for
    // SAMPLER on it makes D3D12 reject the resource.
    info.usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET;
    if (samples == SDL_GPU_SAMPLECOUNT_1) info.usage |= SDL_GPU_TEXTUREUSAGE_SAMPLER;
    info.width = width;
    info.height = height;
    info.layer_count_or_depth = 1;
    info.num_levels = 1;
    info.sample_count = samples;
    SDL_GPUTexture *texture = SDL_CreateGPUTexture(device.handle, &info);
    if (!texture) fatal("SDL_CreateGPUTexture (color)");
    return texture;
}

}  // namespace opra::gpu
