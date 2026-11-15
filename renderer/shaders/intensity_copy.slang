// An EFB copy to an intensity format (GX_TF_I8): the hardware writes Y = 16 + 0.257 R + 0.504 G + 0.098 B (of 255) for each texel.
// The pass converts the copied colour in place; every channel gets Y, as a GX intensity texture gives it to the TEV.
[[vk::binding(0, 0)]] [[vk::image_format("rgba8")]] RWTexture2D<float4> target : register(u0);
struct Constants { int2 size; };
[[vk::push_constant]] ConstantBuffer<Constants> constants : register(b0);

[numthreads(8, 8, 1)]
void main(uint3 id : SV_DispatchThreadID) {
    int2 p = int2(id.xy);
    if (any(p >= constants.size)) return;
    float y = saturate(dot(target[p].rgb, float3(0.257, 0.504, 0.098)) + 16.0 / 255.0);
    target[p] = float4(y, y, y, y);
}
