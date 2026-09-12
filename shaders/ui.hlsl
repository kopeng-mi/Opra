// Window-space UI shader: one atlas texture, one draw for the whole frame's HUD.
// SDL3 GPU register layout: vertex -> b0 in space1; pixel -> b0 in space3, t0/s0 in space2.
cbuffer UIUniforms : register(b0, space1)
{
    float2 screen;   // pixels
};

struct VSIn
{
    float2 pos   : TEXCOORD0;   // location 0: pixels, y down
    float2 uv    : TEXCOORD1;   // location 1
    float4 color : TEXCOORD2;   // location 2
};

struct VSOut
{
    float4 clip  : SV_Position;
    float2 uv    : TEXCOORD0;
    float4 color : TEXCOORD1;
};

VSOut VSMain(VSIn input)
{
    VSOut o;
    // Pixel space to clip space: y is already down, so no flip.
    float2 ndc = float2(input.pos.x / screen.x * 2.0 - 1.0, 1.0 - input.pos.y / screen.y * 2.0);
    o.clip = float4(ndc, 0.0, 1.0);
    o.uv = input.uv;
    o.color = input.color;
    return o;
}

Texture2D    atlas_texture : register(t0, space2);
SamplerState atlas_sampler : register(s0, space2);

float4 PSMain(VSOut input) : SV_Target
{
    // Glyphs are white with coverage in alpha; solids sample the atlas's white block.
    float4 sampled = atlas_texture.Sample(atlas_sampler, input.uv);
    return float4(input.color.rgb, input.color.a * sampled.a);
}
