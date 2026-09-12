// Instanced 3D mesh shader for SDL3 GPU (D3D12/DXIL).
// SDL3 GPU register layout: vertex -> b0 in space1; pixel -> b0/b1 in space3, t/s in space2.
//
// One vertex shader pair serves every pass: opaque hulls, the additive effect pass and the additive
// backdrop differ only in their blend, depth-write and fragment entry point. The opaque fragment
// stage is Cook-Torrance against the star: a hull on a black field with a single key light is
// exactly the case where a real BRDF (and a normal map) is cheaper than faking it.

// SDL3's D3D12 convention: vertex stage buffers live in space1, pixel stage buffers in space3.
// Both stages want the same values, so the layout is declared twice and pushed twice.
cbuffer MeshUniforms : register(b0, space1)
{
    float4x4 view_proj;        // world -> clip
    float4 light_dir;          // xyz: direction toward the star, w: ambient floor
    float4 tint;               // rgb: ambient tint
    float4 view_dir;           // xyz: camera forward, w: rim strength
    float4 fill_dir;           // xyz: direction toward the fill light, w: fill strength
    float4x4 light_view_proj;  // world -> the star's cascade
    float4 light_color;        // rgb: the star's colour, w: its intensity
    float4 shadow_params;      // x: normal-offset metres, y: depth bias, z: uv texel, w: enabled
};

cbuffer MeshUniformsPS : register(b0, space3)
{
    float4x4 ps_view_proj;
    float4 ps_light_dir;
    float4 ps_tint;
    float4 ps_view_dir;
    float4 ps_fill_dir;
    float4x4 ps_light_view_proj;
    float4 ps_light_color;
    float4 ps_shadow_params;
};

// b1: the run's material. Materials follow meshes, so this is pushed once per run and never splits
// a draw (plan 4.3).
cbuffer MaterialUniformsPS : register(b1, space3)
{
    float4 base_color;          // rgb factor, a: unused
    float4 metallic_roughness;  // x metallic, y roughness
    float4 texture_flags;       // x base colour, y metallic-roughness, z normal, w unlit
};

Texture2D    base_color_texture : register(t0, space2);
Texture2D    metal_rough_texture : register(t1, space2);
Texture2D    normal_texture : register(t2, space2);
Texture2D    shadow_map : register(t3, space2);
SamplerState mesh_sampler : register(s0, space2);

// The exporter authors every engine flame and RCS jet as a cone with its apex 36 units behind the
// nozzle, so the fade runs along the raw vertex y before the instance scale squashes it.
static const float FLAME_LENGTH = 36.0;
static const float FLAME_HALF = 18.0;
static const float PI = 3.14159265358979;

struct VSIn
{
    float3 pos      : TEXCOORD0;   // location 0, vertex buffer 0
    float3 normal   : TEXCOORD1;   // location 1, vertex buffer 0
    float4 tangent  : TEXCOORD2;   // location 2: xyz + handedness in w
    float2 uv       : TEXCOORD3;   // location 3
    float3 vcolor   : TEXCOORD4;   // location 4: baked tone
    float4 i_pos    : TEXCOORD5;   // location 5, instance: xyz position
    float4 i_rot    : TEXCOORD6;   // location 6, instance: quaternion xyzw
    float4 i_scale  : TEXCOORD7;   // location 7, instance: xyz half extents
    float4 i_color  : TEXCOORD8;   // location 8, instance: rgb + alpha
};

struct VSOut
{
    float4 clip         : SV_Position;
    float3 world_normal : TEXCOORD0;
    float4 world_tangent : TEXCOORD1;  // xyz + handedness
    float2 uv           : TEXCOORD2;
    float4 color        : TEXCOORD3;
    float3 world_pos    : TEXCOORD4;
};

float3 rotate_quat(float3 v, float4 q)
{
    return v + 2.0 * cross(q.xyz, cross(q.xyz, v) + q.w * v);
}

VSOut VSMain(VSIn input)
{
    VSOut o;

    float3 local = rotate_quat(input.pos * input.i_scale.xyz, input.i_rot);
    float3 world = local + input.i_pos.xyz;

    o.clip = mul(view_proj, float4(world, 1.0));

    // The normal is scaled by the inverse of the per-axis extent, so a squashed part
    // does not shade as if it were square. The tangent rides along and the fragment stage
    // re-orthonormalises it: an instance scale is not a rotation, so a baked tangent basis would
    // still be wrong.
    o.world_normal = rotate_quat(input.normal / (input.i_scale.xyz + 1e-4), input.i_rot);
    o.world_tangent = float4(rotate_quat(input.tangent.xyz, input.i_rot), input.tangent.w);

    // The instance's own alpha, untouched: opaque hulls ignore it, and the additive backdrop
    // (stars, nebula, grit) is what it is authored to be. Only flames get the exhaust profile.
    o.color = float4(input.vcolor * input.i_color.rgb, input.i_color.a);

    o.uv = input.uv;
    o.world_pos = world;
    return o;
}

// Exhaust fades to nothing at the tip: a = pow(1 - t, 1.65), the original fragment shader's
// curve. The exporter authors every engine flame and RCS jet as a cone with its apex 36 units
// behind the nozzle, so the fade runs along the raw vertex y before the instance scale squashes it.
VSOut VSFlame(VSIn input)
{
    VSOut o = VSMain(input);

    float t = saturate((input.pos.y + FLAME_HALF) / FLAME_LENGTH);
    o.color.a = input.i_color.a * pow(saturate(1.0 - t), 1.65);

    return o;
}

/**
 * The star's cascade, sampled with a normal offset and three-by-three PCF. The offset is what keeps
 * the bias small enough that a hull's own shadow does not detach from it (plan 4.6: no peter-pan);
 * the bias only has to cover the depth quantisation the offset leaves behind.
 */
float shadow_term(float3 world, float3 n, float ndl)
{
    if (ps_shadow_params.w < 0.5) return 1.0;
    float4 light = mul(ps_light_view_proj, float4(world + n * ps_shadow_params.x, 1.0));
    if (light.w <= 0.0) return 1.0;
    float2 uv = light.xy / light.w * float2(0.5, -0.5) + 0.5;
    if (uv.x < 0.0 || uv.y < 0.0 || uv.x > 1.0 || uv.y > 1.0) return 1.0;
    float depth = light.z / light.w;
    // Grazing faces need more slack than face-on ones; the offset already covers most of it.
    float bias = ps_shadow_params.y * (1.0 + 2.0 * (1.0 - ndl));
    float texel = ps_shadow_params.z;
    float sum = 0.0;
    for (int y = -1; y <= 1; ++y)
    {
        for (int x = -1; x <= 1; ++x)
        {
            float stored = shadow_map.SampleLevel(mesh_sampler, uv + float2(x, y) * texel, 0);
            sum += (depth - bias > stored) ? 0.0 : 1.0;
        }
    }
    return sum * (1.0 / 9.0);
}

/** Cook-Torrance: GGX distribution, Smith visibility, Schlick Fresnel. */
float3 shade_pbr(VSOut input, float3 albedo, float metallic, float roughness,
                 float3 normal_world)
{
    const float3 n = normal_world;
    const float3 v = -normalize(ps_view_dir.xyz);
    const float3 l = normalize(ps_light_dir.xyz);
    const float3 h = normalize(v + l);
    const float ndl = saturate(dot(n, l));
    const float ndv = saturate(dot(n, v)) + 1e-4;
    const float ndh = saturate(dot(n, h));
    const float vdh = saturate(dot(v, h));

    const float a = roughness * roughness;
    const float a2 = a * a;
    const float d = a2 / (PI * pow(ndh * ndh * (a2 - 1.0) + 1.0, 2.0));
    const float k = (roughness + 1.0) * (roughness + 1.0) / 8.0;
    const float g = (ndv / (ndv * (1.0 - k) + k)) * (ndl / (ndl * (1.0 - k) + k));
    const float3 f0 = lerp(float3(0.04, 0.04, 0.04), albedo, metallic);
    const float3 f = f0 + (1.0 - f0) * pow(1.0 - vdh, 5.0);
    const float3 spec = d * g * f / (4.0 * ndv * ndl + 1e-4);
    const float3 diffuse = (1.0 - f) * (1.0 - metallic) * albedo / PI;

    const float shadow = shadow_term(input.world_pos, n, ndl);
    float3 lit = (diffuse + spec) * ps_light_color.rgb * ps_light_color.w * ndl * shadow;

    // A hemispheric ambient stands in for an IBL: zenith tint at the top, a darker floor below.
    const float up = n.z * 0.5 + 0.5;
    const float3 ambient = ps_tint.rgb * lerp(ps_light_dir.w * 0.55, ps_light_dir.w, up);

    // The fill light keeps the shadowed faces readable, not lit, and it is shadowed too.
    const float fill = saturate(dot(n, normalize(ps_fill_dir.xyz))) * ps_fill_dir.w;
    lit += ps_tint.rgb * fill * shadow * 0.5;

    // Rim light: the edge turning away from the camera picks up a thin highlight, which is what
    // keeps a dark hull legible against the sector's near-black background.
    const float facing = saturate(dot(n, -normalize(ps_view_dir.xyz)));
    const float rim = pow(1.0 - facing, 3.0) * ps_view_dir.w;

    return lit + albedo * ambient + rim * ps_tint.rgb;
}

float4 PSMain(VSOut input) : SV_Target
{
    float3 n = normalize(input.world_normal);
    // Gram-Schmidt against the interpolated normal: the vertex basis is per face, the shading
    // frame has to be per pixel.
    float3 t = input.world_tangent.xyz - n * dot(n, input.world_tangent.xyz);
    float tlen = length(t);
    t = tlen > 1e-5 ? t / tlen : float3(1.0, 0.0, 0.0);
    float3 b = cross(n, t) * (input.world_tangent.w < 0.0 ? -1.0 : 1.0);

    float4 base = base_color;
    if (texture_flags.x > 0.5) base *= base_color_texture.Sample(mesh_sampler, input.uv);
    float metallic = metallic_roughness.x;
    float roughness = max(metallic_roughness.y, 0.04);
    if (texture_flags.y > 0.5)
    {
        float4 mr = metal_rough_texture.Sample(mesh_sampler, input.uv);
        roughness = max(mr.g * metallic_roughness.y, 0.04);
        metallic = mr.b * metallic_roughness.x;
    }
    if (texture_flags.z > 0.5)
    {
        float3 mapped = normal_texture.Sample(mesh_sampler, input.uv).xyz * 2.0 - 1.0;
        n = normalize(t * mapped.x + b * mapped.y + n * mapped.z);
    }

    float3 albedo = base.rgb * input.color.rgb;
    float3 shaded;
    if (texture_flags.w > 0.5)
    {
        // KHR_materials_unlit: a lamp or a glass pane emits its own colour and takes no light.
        shaded = albedo;
    }
    else
    {
        shaded = shade_pbr(input, albedo, metallic, roughness, n);
    }

    float4 result;
    result.rgb = shaded;
    result.a = base.a * input.color.a;
    return result;
}

/**
 * The additive passes' shading: one key, one fill, a rim and an ambient floor, exactly the look the
 * hulls had before the PBR path existed. Flames and the nebula are light sources, not surfaces, and
 * running them through a BRDF would make a flame's brightness depend on the normal of a cone.
 */
float4 PSAmbient(VSOut input) : SV_Target
{
    float3 n = normalize(input.world_normal);

    float key = saturate(dot(n, normalize(ps_light_dir.xyz)));
    float fill = saturate(dot(n, normalize(ps_fill_dir.xyz))) * ps_fill_dir.w;
    float3 lit = ps_tint.rgb * (ps_light_dir.w + (1.0 - ps_light_dir.w) * max(key, fill));

    float facing = saturate(dot(n, -normalize(ps_view_dir.xyz)));
    float rim = pow(1.0 - facing, 3.0) * ps_view_dir.w;

    float4 result;
    // The backdrop is authored to be quiet: its own tint is the whole of its brightness, and a
    // light source that lit the sky would be a second sun.
    result.rgb = input.color.rgb * min(lit, 1.0) + rim * ps_tint.rgb;
    result.a = input.color.a;
    return result;
}
