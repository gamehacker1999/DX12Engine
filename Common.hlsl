// Hit information, aka ray payload
// This sample only carries a shading color and hit distance.
// Note that the payload should be kept as small as possible,
// and that its size must be declared in the corresponding
// D3D12_RAYTRACING_SHADER_CONFIG pipeline subobjet.
struct HitInfo
{
  float4 colorAndDistance;
  float rayDepth;
};

// Attributes output by the raytracing when hitting a surface,
// here the barycentric coordinates
struct Attributes
{
  float2 bary;
};

//since the plane needs to case shadow rays, we need the scene bvh and the shadow ray payload
struct ShadowHitInfo
{
    bool isHit; // this is tha payload of the ray, we just want to know if we hit something or not
};
