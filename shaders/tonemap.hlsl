// Tonemap and grade: ACES (the Narkowicz fit) from the HDR composite onto the swapchain, with the
// sRGB transfer. The swapchain is an UNORM target, so this shader is the only place the frame
// becomes display-referred - and the UI pass that follows it is deliberately outside it, because
// the HUD is authored in display space and a tonemapped #DCE6E8 is not #DCE6E8 (plan 4.1).

cbuffer GradeUniforms : register(b0, space3)
{
    float4 params;  // x exposure, y unused, z saturation, w contrast
};

Texture2D    hdr_texture : register(t0, space2);
SamplerState grade_sampler : register(s0, space2);

struct FSOut
{
    float4 clip : SV_Position;
    float2 uv   : TEXCOORD0;
};

/** The fullscreen triangle lives in bloom.hlsl; this file only needs the pixel stage. */

float3 aces(float3 x)
{
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    return saturate((x * (a * x + b)) / (x * (c * x + d) + e));
}

float4 PSMain(FSOut input) : SV_Target
{
    float3 color = hdr_texture.SampleLevel(grade_sampler, input.uv, 0).rgb * params.x;
    color = aces(color);

    // Grade: saturation about the luminance, then a gentle contrast about mid grey. Both stay
    // small: the image is already what the palette says it is.
    const float lum = dot(color, float3(0.2126, 0.7152, 0.0722));
    color = saturate(lerp(float3(lum, lum, lum), color, params.z));
    color = saturate((color - 0.5) * params.w + 0.5);

    // Linear to sRGB, the exact curve rather than a 2.2 power: the palette's hex values were chosen
    // against this transfer. Component-wise, so it is a select and not a branch.
    color = select(color <= 0.0031308, color * 12.92,
                   1.055 * pow(max(color, 1e-5), 1.0 / 2.4) - 0.055);
    return float4(color, 1.0);
}
