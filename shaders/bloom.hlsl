// The bloom chain (plan 4.1): a soft-knee threshold into the first level, five downsamples, five
// tent upsamples that add back into the level below, then a combine. The star and the drive flames
// sit above the threshold; the hulls, the chart ink and the nebula sit under it, and the fact that
// the grade never touches them is the point - contrast comes from the lights, not the background.
//
// One vertex stage builds a fullscreen triangle from the vertex id: there is no vertex buffer in
// this pass, and a triangle covers the target with one primitive fewer than a quad.

cbuffer BloomUniforms : register(b0, space3)
{
    float4 texel;   // xy source texel, zw target texel
    float4 params;  // x threshold, y knee, z strength, w unused
};

Texture2D    source_texture : register(t0, space2);
Texture2D    bloom_texture : register(t1, space2);
SamplerState bloom_sampler : register(s0, space2);

struct FSOut
{
    float4 clip : SV_Position;
    float2 uv   : TEXCOORD0;
};

FSOut VSFullscreen(uint id : SV_VertexID)
{
    FSOut o;
    const float2 uv = float2((id << 1) & 2, id & 2);
    o.uv = uv;
    o.clip = float4(uv * float2(2.0, -2.0) + float2(-1.0, 1.0), 0.0, 1.0);
    return o;
}

float3 source_at(float2 uv)
{
    return source_texture.SampleLevel(bloom_sampler, uv, 0).rgb;
}

/** Threshold with a soft knee: a hard cut makes a star's edge pop as the exposure moves. */
float4 PSThreshold(FSOut input) : SV_Target
{
    const float3 c = source_at(input.uv);
    const float brightness = max(c.r, max(c.g, c.b));
    const float knee = max(params.y, 1e-4);
    float soft = clamp(brightness - params.x + knee, 0.0, 2.0 * knee);
    soft = soft * soft / (4.0 * knee);
    const float contribution = max(soft, brightness - params.x) / max(brightness, 1e-4);
    return float4(c * contribution, 1.0);
}

/** The classic thirteen-tap box: a plain 4-tap average lets one bright texel crawl between levels. */
float4 PSDownsample(FSOut input) : SV_Target
{
    const float2 t = texel.xy;
    float3 sum = source_at(input.uv + t * float2(-2.0, 2.0));
    sum += source_at(input.uv + t * float2(0.0, 2.0)) * 2.0;
    sum += source_at(input.uv + t * float2(2.0, 2.0));
    sum += source_at(input.uv + t * float2(-2.0, 0.0)) * 2.0;
    sum += source_at(input.uv) * 4.0;
    sum += source_at(input.uv + t * float2(2.0, 0.0)) * 2.0;
    sum += source_at(input.uv + t * float2(-2.0, -2.0));
    sum += source_at(input.uv + t * float2(0.0, -2.0)) * 2.0;
    sum += source_at(input.uv + t * float2(2.0, -2.0));
    return float4(sum / 16.0, 1.0);
}

/** A 3x3 tent, added into the level below: the sum of the levels is one wide, smooth halo. */
float4 PSUpsample(FSOut input) : SV_Target
{
    const float2 t = texel.xy;
    float3 sum = source_at(input.uv + float2(-t.x, -t.y));
    sum += source_at(input.uv + float2(0.0, -t.y)) * 2.0;
    sum += source_at(input.uv + float2(t.x, -t.y));
    sum += source_at(input.uv + float2(-t.x, 0.0)) * 2.0;
    sum += source_at(input.uv) * 4.0;
    sum += source_at(input.uv + float2(t.x, 0.0)) * 2.0;
    sum += source_at(input.uv + float2(-t.x, t.y));
    sum += source_at(input.uv + float2(0.0, t.y)) * 2.0;
    sum += source_at(input.uv + float2(t.x, t.y));
    return float4(sum / 16.0 * params.z, 1.0);
}

/** The scene as it was, plus the bloom chain: the last pass before the tonemap. */
float4 PSCombine(FSOut input) : SV_Target
{
    const float3 scene = source_at(input.uv);
    const float3 bloom = bloom_texture.SampleLevel(bloom_sampler, input.uv, 0).rgb;
    return float4(scene + bloom * params.z, 1.0);
}
