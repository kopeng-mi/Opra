// A planet is a sphere plus this shader, never a textured mesh: it has to read correctly from a
// two-pixel dot on the map to filling the frame on descent (plan 4.4). The LOD is the shader's own,
// chosen from the body's angular size in pixels by the renderer:
//
//   class 0  a disc with a colour       (under 4 px across)
//   class 1  a shaded sphere            (under 40 px)
//   class 2  terminator, triplanar surface, cloud bands, night side
//
// The air is a second, additive draw at the shell radius (PSAir): an atmosphere is a volume the
// line of sight passes through, not a surface, and blending it over the disc is what makes the limb
// brighter than the middle.
//
// The surface heightfield is sim/terrain.cpp's, bit for bit: the silhouette seen from orbit has to
// be the ground the collision code samples, or landing is a lie.

// SDL3's D3D12 convention: vertex stage buffers live in space1, pixel stage buffers in space3.
cbuffer BodyUniforms : register(b0, space1)
{
    float4x4 view_proj;
    float4 center_radius;   // xyz centre (scene units), w drawn radius
    float4 color;           // rgb albedo
    float4 terrain;         // x seed, y amplitude over the radius, z class, w clock (seconds)
    float4 atmosphere;      // x scale height over the radius, y air top over the radius, z star angle
    float4 star_direction;  // xyz toward the star, w intensity
    float4 star_color;      // rgb
    float4 viewport;        // xy pixels, z star pixel radius, w this body's pixel radius
    float4 eye_position;    // xyz the camera eye
    float4 maps;            // x albedo, y clouds, z night, w unused
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

// The body's maps (plan-04 s3.4): albedo first, then the cloud deck and the night lights. The
// air shell's own fragment stage declares none of these.
Texture2D    albedo_map : register(t0, space2);
Texture2D    cloud_map : register(t1, space2);
Texture2D    night_map : register(t2, space2);
SamplerState body_sampler : register(s0, space2);

static const float PI = 3.14159265358979;
/** The drawn air shell never thinner than this share of the radius: Tessera's own air is 1.7% of
 *  its radius, which is correct and sub-pixel on a map glyph, so the rim gets a floor. */
static const float AIR_FLOOR = 0.06;

struct VSIn
{
    float3 pos     : TEXCOORD0;   // location 0: the unit sphere
    float4 i_pos   : TEXCOORD5;   // location 5, instance: xyz centre
    float4 i_rot   : TEXCOORD6;
    float4 i_scale : TEXCOORD7;   // location 7, instance: xyz radius
};

struct VSOut
{
    float4 clip         : SV_Position;
    float3 world_pos    : TEXCOORD0;
    float3 world_normal : TEXCOORD1;
    float3 unit         : TEXCOORD2;   // the unit-sphere point: latitude, longitude, triplanar
    float4 center_clip  : TEXCOORD3;
};

float3 rotate_quat(float3 v, float4 q)
{
    return v + 2.0 * cross(q.xyz, cross(q.xyz, v) + q.w * v);
}

/** sim/terrain.cpp's hash, exactly: asuint on a signed int, because negative theta happens. */
float hash01(int i, uint seed)
{
    uint h = asuint(i) * 747796405u + seed * 2891336453u;
    h ^= h >> 15;
    h *= 2246822519u;
    h ^= h >> 13;
    h *= 3266489917u;
    h ^= h >> 16;
    return float(h) * (1.0 / 4294967296.0);
}

float value_noise(float x, uint seed)
{
    int i = (int)floor(x);
    float t = x - float(i);
    float s = t * t * (3.0 - 2.0 * t);
    float a = hash01(i, seed);
    float b = hash01(i + 1, seed);
    return a + (b - a) * s;
}

/** h(theta) = sum over o of A * g^o * value_noise(theta * 2^o * f + phase_o), g = 0.55, f = 1.0. */
float terrain_height(float theta, uint seed)
{
    float h = 0.0;
    float gain = 1.0;
    float frequency = 1.0;
    for (int o = 0; o < 6; ++o)
    {
        h += gain * value_noise(theta * frequency + float(o) * 0.6180339887, seed);
        gain *= 0.55;
        frequency *= 2.0;
    }
    return h;
}

/** A 2D value noise for everything that is not the terrain: cloud bands and the surface grain. */
float hash01_at(int2 at, uint seed)
{
    return hash01(at.x * 1973 + at.y * 9277, seed);
}

float value_noise2(float2 p, uint seed)
{
    float2 cell = floor(p);
    float2 f = p - cell;
    f = f * f * (3.0 - 2.0 * f);
    const int2 at = int2(cell);
    float a = hash01_at(at, seed);
    float b = hash01_at(at + int2(1, 0), seed);
    float c = hash01_at(at + int2(0, 1), seed);
    float d = hash01_at(at + int2(1, 1), seed);
    return lerp(lerp(a, b, f.x), lerp(c, d, f.x), f.y);
}

float fbm(float2 p, uint seed)
{
    float sum = 0.0;
    float amp = 0.5;
    for (int o = 0; o < 5; ++o)
    {
        sum += amp * value_noise2(p, seed + uint(o) * 17u);
        p *= 2.03;
        amp *= 0.5;
    }
    return sum;
}

VSOut VSMain(VSIn input)
{
    VSOut o;

    const float radius = center_radius.w > 0.0 ? center_radius.w : 1.0;
    float3 local = rotate_quat(input.pos * input.i_scale.xyz, input.i_rot);
    float3 world = local + input.i_pos.xyz;
    float3 normal = normalize(rotate_quat(input.pos, input.i_rot));

    // The heightfield is radial about the body's own axis, which is the plane's normal: the body is
    // a surface of revolution in the play plane, so displacing in-plane is what puts the true
    // silhouette at the limb the ship sees from orbit. Pads are flat spans sim/terrain.cpp forces,
    // and a shader may skip that detail until the descent (plan 4.4).
    if (terrain.z > 1.5 && terrain.y > 0.0)
    {
        const float2 flat = local.xy;
        const float reach = length(flat);
        if (reach > radius * 0.05)
        {
            const float theta = atan2(local.y, local.x);
            const uint seed = uint(terrain.x + 0.5);
            world += float3(flat / reach, 0.0) * (terrain_height(theta, seed) * radius * terrain.y);
        }
    }

    o.unit = input.pos;
    o.world_normal = normal;
    o.world_pos = world;
    o.center_clip = mul(view_proj, float4(center_radius.xyz, 1.0));
    o.clip = mul(view_proj, float4(world, 1.0));
    return o;
}

/** The cloud deck's own colour: overcast white, a shade under the star's own. */
float3 cloud_tint()
{
    return float3(0.92, 0.94, 0.97);
}

/** Pixels from the disc's centre, in disc radii: 1 is the limb, whatever the body's size. */
float screen_radius(VSOut input)
{
    const float2 centre_ndc = input.center_clip.xy / input.center_clip.w;
    const float2 centre_px = (centre_ndc * float2(0.5, -0.5) + 0.5) * ps_viewport.xy;
    return length(input.clip.xy - centre_px) / max(ps_viewport.w, 1.0);
}

/** Triplanar grain, in units of the body's own radius: no UVs, no seams, no polar pinch. */
float surface_grain(float3 offset, uint seed)
{
    const float3 p = offset / max(ps_center_radius.w, 1e-3) * 3.0;
    const float3 w = abs(normalize(offset));
    const float3 blend = w / max(w.x + w.y + w.z, 1e-4);
    return blend.x * fbm(p.yz, seed) + blend.y * fbm(p.xz, seed + 3u) +
           blend.z * fbm(p.xy, seed + 11u);
}

float4 PSMain(VSOut input) : SV_Target
{
    const float lod = ps_terrain.z;
    const float3 albedo = ps_color.rgb;
    // The body's equirectangular maps, in the plane's own axes: latitude from z, longitude from
    // (y, x), so a map's x runs east and its y runs pole to pole (plan-04 s3.4). The sampler
    // wraps in longitude and clamps at the poles, and the mips carry a dot on the map screen and
    // a limb on descent from the same 2048-pixel file.
    const float3 direction = normalize(input.unit);
    const float2 map_uv = float2(atan2(direction.y, direction.x) / (2.0 * PI) + 0.5,
                                 0.5 - asin(clamp(direction.z, -1.0, 1.0)) / PI);

    if (lod < 0.5)
    {
        // A dot on the map: the body's colour and nothing else. A 2-pixel sphere would be a lie in
        // the opposite direction - the shape is not resolvable, the colour is.
        return float4(albedo, 1.0);
    }

    const float3 n = normalize(input.world_normal);
    const float3 l = normalize(ps_star_direction.xyz);
    const float ndl = dot(n, l);
    const float3 to_eye = normalize(ps_eye_position.xyz - input.world_pos);
    const float ndv = saturate(dot(n, to_eye));

    // The terminator wraps by the star's angular radius: a body deep in its star's well has a soft
    // edge, one at the system's rim a hard one.
    const float wrap = clamp(ps_atmosphere.z * 8.0, 0.01, 0.6);
    const float term = saturate((ndl + wrap) / (1.0 + wrap));

    // The albedo: the body's own map where it has one, the flat tint and procedural grain where
    // it does not (an airless moon stays shaded the old way). The map is albedo only - the height
    // the ship lands on stays sim/terrain.cpp's, so the silhouette is never a lie (s3.4).
    float3 surface = albedo;
    if (ps_maps.x > 0.5)
    {
        surface = albedo_map.Sample(body_sampler, map_uv).rgb;
    }

    if (lod < 1.5)
    {
        // A small sphere: the terminator and a little limb darkening, nothing that would be noise
        // at twenty pixels.
        float3 lit = surface * term * ps_star_color.rgb * ps_star_direction.w;
        lit *= pow(ndv, 0.45);
        return float4(lit + surface * 0.02, 1.0);
    }

    const uint seed = uint(ps_terrain.x + 0.5);
    if (ps_maps.x > 0.5)
    {
        // The map carries its own texture; the procedural grain would fight it.
        float3 lit = surface;
        lit *= term * ps_star_color.rgb * ps_star_direction.w;
        lit *= pow(ndv, 0.45);

        // The cloud deck: the body's own map, scrolling at a fixed offset rate under the
        // surface's rotation - a weather layer, not a second body. The map's red channel is the
        // coverage; the gate keeps it off the poles.
        if (ps_maps.y > 0.5)
        {
            const float drift = ps_terrain.w * 0.0045;
            const float4 cover = cloud_map.Sample(body_sampler, map_uv - float2(drift, 0.0));
            const float gate = 0.55 + 0.45 * sin(asin(clamp(direction.z, -1.0, 1.0)) * 3.0);
            const float clouds = smoothstep(0.30, 0.72, cover.r * gate) * saturate(term * 1.4);
            lit = lerp(lit, cloud_tint() * term * ps_star_color.rgb * ps_star_direction.w,
                       clouds * 0.85);
        }

        // Night lights: the city glow, masked to the dark hemisphere, emissive - added here,
        // before bloom, so it survives the tonemap (s3.4).
        if (ps_maps.z > 0.5)
        {
            const float night = saturate(-ndl);
            lit += night_map.Sample(body_sampler, map_uv).rgb * (night * 2.4);
        }
        return float4(lit, 1.0);
    }

    float3 lit = albedo * (0.82 + 0.36 * surface_grain(input.world_pos - ps_center_radius.xyz, seed));
    lit *= term * ps_star_color.rgb * ps_star_direction.w;
    lit *= pow(ndv, 0.45);

    // Two scrolling FBM bands in a latitude-banded coordinate: the bands are why a cloud deck reads
    // as weather rather than as noise, and the gate keeps them off the poles.
    const float latitude = asin(clamp(direction.z, -1.0, 1.0));
    const float longitude = atan2(direction.y, direction.x);
    const float2 band = float2(longitude * 2.4, latitude * 6.0);
    const float drift = ps_terrain.w * 0.006;
    const float low = fbm(band + float2(drift, 0.0), seed + 41u);
    const float high = fbm(band * 1.7 + float2(-drift * 0.6, 3.1), seed + 59u);
    const float gate = 0.55 + 0.45 * sin(latitude * 3.0);
    const float clouds = smoothstep(0.46, 0.72, (low * 0.6 + high * 0.4) * gate) * saturate(term * 1.4);
    lit = lerp(lit, cloud_tint() * term * ps_star_color.rgb * ps_star_direction.w, clouds * 0.85);

    // Night side: a city glow, masked to the dark hemisphere and kept very dim - the lit side is
    // the subject, and a night side that competes with it is a bug.
    const float night = saturate(-ndl);
    lit += float3(1.0, 0.72, 0.42) * (night * 0.03 * smoothstep(0.35, 0.75, low));
    return float4(lit, 1.0);
}

/** The cloud deck's own colour: overcast white, a shade under the star's own. */
float3 white_tint()
{
    return float3(0.92, 0.94, 0.97);
}

/**
 * The air shell: an additive pass at the shell radius, over and around the disc. The optical depth
 * is the scale height, the tint is Rayleigh-ish, and the shell is drawn thick enough to survive a
 * twenty-pixel disc (AIR_FLOOR).
 */
float4 PSAir(VSOut input) : SV_Target
{
    const float r = screen_radius(input);
    const float thickness = max(ps_atmosphere.y, AIR_FLOOR);
    const float shell = 1.0 + thickness;
    if (r > shell) discard;

    const float height = max(ps_atmosphere.x, 1e-3);
    // Exponential falloff from the limb, and the chord through the shell: a grazing sight line
    // crosses more air than a radial one, which is what brightens the limb.
    float depth = exp(-(r - 1.0) / height);
    depth *= saturate((shell - r) / thickness);
    const float chord = sqrt(saturate(shell * shell - r * r)) * 2.0;
    depth *= 0.35 + 0.65 * chord;

    const float3 n = normalize(input.world_normal);
    const float sun = saturate(dot(n, normalize(ps_star_direction.xyz)) + 0.35);
    // Rayleigh-ish weighting: short wavelengths scatter, so the rim is blue and the terminator
    // warms towards red as the sight line lengthens.
    const float3 rayleigh = lerp(float3(1.0, 0.45, 0.22), float3(0.32, 0.55, 1.0), saturate(sun));
    const float3 emitted = rayleigh * depth * sun * ps_star_color.rgb * ps_star_direction.w * 9.0;
    return float4(emitted, saturate(depth * 2.0));
}
