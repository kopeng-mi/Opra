#include "render/texture.h"

// Header-only: the implementations live here, in the one module that owns the loader.
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_RESIZE2_IMPLEMENTATION

#include <algorithm>
#include <cmath>
#include <cstring>

#include <stb_image.h>
#include <stb_image_resize2.h>

#include "core/log.h"

namespace opra::render {
namespace {

/** Largest power of two not greater than `value`: a mip chain steps down cleanly. */
int floor_power_of_two(int value) {
    int pot = 1;
    while (pot * 2 <= value && pot < 1 << 14) pot *= 2;
    return pot;
}

Uint32 mip_levels(int width, int height) {
    const int largest = std::max(width, height);
    return largest > 1 ? static_cast<Uint32>(std::log2(static_cast<double>(largest))) + 1 : 1;
}

}  // namespace

SDL_GPUTextureFormat map_format(bool srgb) {
    // The format IS the transfer function: sampling an sRGB texture returns linear light,
    // pushing a data map through it would corrupt the data (plan-04 s3.3).
    return srgb ? SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM_SRGB
                : SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
}

LoadedTexture load_texture(gpu::Device &device, const std::string &path, bool srgb) {
    int width = 0;
    int height = 0;
    int channels = 0;
    // Four channels always: a source without alpha gets one, and the GPU format never has to
    // branch on what the PNG happened to carry.
    stbi_uc *loaded = stbi_load(path.c_str(), &width, &height, &channels, 4);
    if (!loaded) {
        SDL_Log("load_texture(%s): %s", path.c_str(), stbi_failure_reason());
        fatal("stbi_load");
    }
    const int pot_width = floor_power_of_two(width);
    const int pot_height = floor_power_of_two(height);
    stbi_uc *pixels = loaded;
    if (pot_width != width || pot_height != height) {
        // stbir's byte path: RGBA8 in, RGBA8 out, straight resize to the power-of-two.
        stbi_uc *resized = static_cast<stbi_uc *>(
            std::malloc(static_cast<size_t>(pot_width) * pot_height * 4));
        if (!resized) fatal("stbir resize allocation");
        stbir_resize_uint8_linear(loaded, width, height, 0, resized, pot_width, pot_height, 0,
                                  STBIR_RGBA);
        stbi_image_free(loaded);
        pixels = resized;
        width = pot_width;
        height = pot_height;
    }

    SDL_GPUTextureCreateInfo info{};
    info.type = SDL_GPU_TEXTURETYPE_2D;
    info.format = map_format(srgb);
    info.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER | SDL_GPU_TEXTUREUSAGE_COLOR_TARGET;
    info.width = static_cast<Uint32>(width);
    info.height = static_cast<Uint32>(height);
    info.layer_count_or_depth = 1;
    info.num_levels = mip_levels(width, height);
    SDL_GPUTexture *texture = SDL_CreateGPUTexture(device.handle, &info);
    if (!texture) fatal("SDL_CreateGPUTexture (map)");
    const Uint32 bytes = static_cast<Uint32>(width) * height * 4;
    SDL_GPUTransferBufferCreateInfo transfer_info{};
    transfer_info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    transfer_info.size = bytes;
    SDL_GPUTransferBuffer *transfer = SDL_CreateGPUTransferBuffer(device.handle, &transfer_info);
    if (!transfer) fatal("SDL_CreateGPUTransferBuffer (map)");
    void *mapped = SDL_MapGPUTransferBuffer(device.handle, transfer, false);
    std::memcpy(mapped, pixels, bytes);
    SDL_UnmapGPUTransferBuffer(device.handle, transfer);
    stbi_image_free(pixels);

    SDL_GPUCommandBuffer *cmd = SDL_AcquireGPUCommandBuffer(device.handle);
    SDL_GPUCopyPass *copy = SDL_BeginGPUCopyPass(cmd);
    SDL_GPUTextureTransferInfo source{};
    source.transfer_buffer = transfer;
    source.offset = 0;
    source.pixels_per_row = static_cast<Uint32>(width);
    source.rows_per_layer = static_cast<Uint32>(height);
    SDL_GPUTextureRegion destination{};
    destination.texture = texture;
    destination.w = static_cast<Uint32>(width);
    destination.h = static_cast<Uint32>(height);
    destination.d = 1;
    SDL_UploadToGPUTexture(copy, &source, &destination, false);
    SDL_EndGPUCopyPass(copy);
    // A planet is 2 px wide on the map screen and 2000 px on descent: without the chain the 2 px
    // version is one arbitrarily-sampled texel that flickers as it rotates (plan-04 s3.3).
    SDL_GenerateMipmapsForGPUTexture(cmd, texture);
    if (!gpu::submit(cmd)) fatal("SDL_SubmitGPUCommandBuffer (map upload)");
    SDL_ReleaseGPUTransferBuffer(device.handle, transfer);

    return LoadedTexture{texture, width, height};
}

void destroy_texture(gpu::Device &device, const LoadedTexture &texture) {
    if (texture.texture) SDL_ReleaseGPUTexture(device.handle, texture.texture);
}

}  // namespace opra::render
