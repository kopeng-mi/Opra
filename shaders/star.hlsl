// The star: a limb-darkened disc with a corona that falls off as 1/r² in screen space, drawn
// additively so the bloom chain finds it (plan 4.4). The geometry is the same unit sphere the
// planets use, scaled past the disc to give the corona somewhere to live, and the disc itself is
// solved from the screen radius rather than from the mesh: a limb-darkened profile is an analytic
// function of mu, and interpolating it across a coarse sphere would band.
//
// Intensity is HDR - the tonemap and the bloom threshold are what turn it into a sun.

cbuffer BodyUniforms : register(b0, space1)
{
    float4x4 view_proj;
    float4 center_radius;   // xyz centre, w drawn radius
    float4 color;           // rgb the star's own colour
    float4 terrain;         // z class, w clock
    float4 atmosphere;      // z star angular radius
    float4 star_direction;  // w intensity
    float4 star_color;
    float4 viewport;        // xy pixels, z star pixel radius, w this star's pixel radius
    float4 eye_position;
    float4 maps;            // x photosphere bound, w unused
};

cbuffer BodyUniformsPS : register(b0, space3)
{
    float4x4 ps_view_proj;
    float4 ps_center_radius;
    float4 ps_color;
    float4 ps_terrain;
    float4 ps_atmosphere;
    float4 ps_star_direction;
    float4 ps_star_color;
    float4 ps_viewport;
    float4 ps_eye_position;
    float4 ps_maps;
};

// The photosphere tile (plan-04 s3.4), sampled in the star's own axes: two wraps in longitude,
// one span pole to pole, under the limb darkening.
Texture2D    photosphere_map : register(t0, space2);
SamplerState star_sampler : register(s0, space2);

/** The corona is drawn out to here, in disc radii: the geometry is scaled to match. */
static const float PI = 3.14159265358979;
/** The corona is drawn out to here, in disc radii: the geometry is scaled to match. */
static const float CORONA = 3.2;
/** Limb darkening: I(mu) = I0 (1 - u (1 - mu)), the standard Eddington-ish profile. */
static const float LIMB_U = 0.6;

struct VSIn
{
    float3 pos     : TEXCOORD0;   // location 0: the unit sphere
    float4 i_pos   : TEXCOORD5;   // location 5, instance: xyz centre
    float4 i_rot   : TEXCOORD6;
    float4 i_scale : TEXCOORD7;   // location 7, instance: xyz radius
};

struct VSOut
{
    float4 clip        : SV_Position;
    float3 world_pos   : TEXCOORD0;
    float3 unit        : TEXCOORD2;   // the unit-sphere point: the photosphere's own axes
    float4 center_clip : TEXCOORD1;
};

float3 rotate_quat(float3 v, float4 q)
{
    return v + 2.0 * cross(q.xyz, cross(q.xyz, v) + q.w * v);
}

VSOut VSMain(VSIn input)
{
    VSOut o;
    const float3 local = rotate_quat(input.pos * input.i_scale.xyz, input.i_rot);
    const float3 world = local + input.i_pos.xyz;
    o.world_pos = world;
    o.unit = input.pos;
    o.center_clip = mul(view_proj, float4(center_radius.xyz, 1.0));
    o.clip = mul(view_proj, float4(world, 1.0));
    return o;
}

float4 PSMain(VSOut input) : SV_Target
{
    const float2 centre_ndc = input.center_clip.xy / input.center_clip.w;
    const float2 centre_px = (centre_ndc * float2(0.5, -0.5) + 0.5) * ps_viewport.xy;
    // The geometry spans CORONA disc radii, so the mesh's own distance from the centre already
    // gives the ratio: no need to re-project the sphere point.
    const float r = length(input.clip.xy - centre_px) / max(ps_viewport.w, 1.0);
    if (r > CORONA) discard;

    const float3 tint = ps_color.rgb * ps_star_direction.w;

    if (ps_terrain.z < 0.5)
    {
        // Under four pixels across: the disc is the whole of it, and a corona at that size is a
        // smudge. A hard edge and the star's colour.
        if (r > 1.0) discard;
        return float4(tint, 1.0);
    }

    // One pixel of softness at the limb: the disc edge is the sharpest thing in the frame, and a
    // hard aliased edge on an HDR source is a crawling line.
    const float edge = ps_viewport.w > 1.0 ? 1.0 / ps_viewport.w : 1.0;
    if (r < 1.0)
    {
        const float mu = sqrt(saturate(1.0 - r * r));
        const float limb = 1.0 - LIMB_U * (1.0 - mu);
        const float coverage = saturate((1.0 - r) / edge);
        if (ps_maps.x > 0.5)
        {
            // The photosphere's granulation, tiled under the limb: the map carries the surface's
            // own brightness variation, the analytic profile carries the edge.
            const float3 unit = normalize(input.unit);
            const float2 uv = float2(atan2(unit.y, unit.x) / PI + 0.5,
                                     0.5 - asin(clamp(unit.z, -1.0, 1.0)) / PI);
            const float3 granule = photosphere_map.Sample(star_sampler, uv).rgb;
            return float4(tint * limb * (0.72 + 0.56 * granule), coverage);
        }
        return float4(tint * limb, coverage);
    }

    // Corona: 1/r², normalised to reach zero at the geometry's own edge so the mesh's silhouette is
    // not also the corona's cutoff.
    const float falloff = (1.0 / (r * r) - 1.0 / (CORONA * CORONA)) / (1.0 - 1.0 / (CORONA * CORONA));
    return float4(tint * falloff * 0.35, saturate(falloff));
}
