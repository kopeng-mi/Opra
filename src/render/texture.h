// Standalone PNG textures: planet maps, rock tiles and the star photosphere are not model assets
// and must not be forced into a GLB to get on the GPU (plan-04 A3). One loader, one rule it cannot
// get wrong quietly: colour maps are sRGB, data maps are linear - the wrong pairing is the most
// common source of a PBR that "looks off", and it is invisible until it is compared against a
// reference (plan-04 s3.3, review gate 2).
#pragma once

#include <string>

#include "gpu/gpu.h"

namespace opra::render {

/** A loaded image: the GPU texture and the pixel size it was authored at. */
struct LoadedTexture {
    SDL_GPUTexture *texture = nullptr;
    int width = 0;
    int height = 0;
};

/**
 * The texture format a map loads as: the sRGB variant carries the colour transfer, so sampling
 * returns linear light; the plain UNORM one is for data the shader reads as numbers. This is the
 * one pairing plan-04 review gate 2 asks to hold, named so a test can assert it directly.
 */
SDL_GPUTextureFormat map_format(bool srgb);

/**
 * Loads a PNG into a full-mip GPU texture. `srgb` picks the texture format, which is what makes
 * sampling linearize the colour transfer on the GPU: an albedo sampled without it is washed out,
 * a normal map sampled with it is corrupted. The file is resized to power-of-two on the way in,
 * so a mip chain steps down cleanly.
 *
 * Fails hard on a missing or unreadable file: a silently white planet is worse than a crash.
 */
LoadedTexture load_texture(gpu::Device &device, const std::string &path, bool srgb);

void destroy_texture(gpu::Device &device, const LoadedTexture &texture);

}  // namespace opra::render
