// The star's shadow cascade: depth only, one orthographic pass over the instances the scene pass
// would draw opaque (plan 4.1). The vertex stage mirrors mesh.hlsl's input layout so the same
// vertex and instance buffers bind with no extra copies; nothing here reads a normal or a colour,
// because a depth map is a depth map.

cbuffer ShadowUniforms : register(b0, space1)
{
    float4x4 light_view_proj;  // world -> the cascade's depth
};

struct VSIn
{
    // Only these four: a pipeline must supply every input the stage reads, and the shadow pass
    // binds this layout with no normal, tangent, uv or colour buffers at all.
    float3 pos      : TEXCOORD0;   // location 0, vertex buffer 0
    float4 i_pos    : TEXCOORD5;   // location 5, instance: xyz position
    float4 i_rot    : TEXCOORD6;   // location 6, instance: quaternion xyzw
    float4 i_scale  : TEXCOORD7;   // location 7, instance: xyz half extents
};

float3 rotate_quat(float3 v, float4 q)
{
    return v + 2.0 * cross(q.xyz, cross(q.xyz, v) + q.w * v);
}

float4 VSMain(VSIn input) : SV_Position
{
    const float3 local = rotate_quat(input.pos * input.i_scale.xyz, input.i_rot);
    return mul(light_view_proj, float4(local + input.i_pos.xyz, 1.0));
}

// A depth-only cascade still needs a fragment stage: SDL3 GPU builds a graphics pipeline for it,
// and the pass has no colour targets for one to write to.
void PSMain() {}
