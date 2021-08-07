#ifndef COMMON_HLSL
#define COMMON_HLSL

#define WIDTH 1280.0f
#define HEIGHT 720.0f

#define DISPLAY_WIDTH 1920.0f
#define DISPLAY_HEIGHT 1080.0f

// Hit information, aka ray payload
// This sample only carries a shading color and hit distance.
// Note that the payload should be kept as small as possible,
// and that its size must be declared in the corresponding
// D3D12_RAYTRACING_SHADER_CONFIG pipeline subobjet.
struct HitInfo
{
  float3 color;
  float rayDepth;
  uint rndseed;
  float3 currentPosition;
  float3 normal;
  float3 diffuseColor;
};

struct GbufferPayload
{
    float4 roughnessMetallic;
    float3 albedo;
    float3 position;
    float3 normal;
};

struct DirectionalLight
{
    float4 ambientColor;
    float4 diffuse;
    float4 specularity;
    float3 direction;
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
    uint primitiveIndex;
    bool isHit; // this is tha payload of the ray, we just want to know if we hit something or not
};

struct Plane
{
    float3 normal;
    float distance;
};

Plane ComputePlane(float3 p0, float3 p1, float3 p2)
{
    Plane plane;
 
    float3 v0 = p1 - p0;
    float3 v2 = p2 - p0;
    
    float3 normal = normalize(cross(v2, v0));
 
    plane.normal = normal;
 
    // Compute the distance to the origin using p0.
    plane.distance = dot(plane.normal, p0);
 
    return plane;
}

struct Frustum
{
    Plane frustumPlanes[6];
};

struct ViewFrustum
{
    float4 planes[6];
    float3 points[8]; // 0-3 near 4-7 far
};


struct Sphere
{
    float3 c; // Center point.
    float r; // Radius.
};

bool SphereInsidePlane(Sphere sphere, Plane plane)
{
    float3 norm = plane.normal;
    float3 centerPos = sphere.c;
    float ndotc = dot(norm, centerPos);
    
    float sphereRad = sphere.r;
    float planeDist = plane.distance;
    
    return ndotc - plane.distance < -sphere.r;
}

// Check to see of a light is partially contained within the frustum.
bool SphereInsideFrustum(Sphere sphere, Frustum frustum, float zNear, float zFar)
{
    bool result = true;
    
    float sphereZPos = sphere.c.z;
    float sphereRadius = sphere.r;
 
    if (sphereZPos - sphereRadius > zFar || sphereZPos + sphereRadius < zNear)
    {
        result = false;
    }
 
    // Then check frustum planes
    for (int i = 0; i < 4 && result; i++)
    {
        if (SphereInsidePlane(sphere, frustum.frustumPlanes[i]))
        {
            result = false;
        }
    }
 
    return result;
}

struct Cone
{
    float3 T; // Cone tip.
    float h; // Height of the cone.
    float3 d; // Direction of the cone.
    float r; // bottom radius of the cone.
};

#define IDENTITY_MATRIX float4x4(1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1)

matrix InvertMatrix(matrix m)
{
    float n11 = m[0][0], n12 = m[1][0], n13 = m[2][0], n14 = m[3][0];
    float n21 = m[0][1], n22 = m[1][1], n23 = m[2][1], n24 = m[3][1];
    float n31 = m[0][2], n32 = m[1][2], n33 = m[2][2], n34 = m[3][2];
    float n41 = m[0][3], n42 = m[1][3], n43 = m[2][3], n44 = m[3][3];

    float t11 = n23 * n34 * n42 - n24 * n33 * n42 + n24 * n32 * n43 - n22 * n34 * n43 - n23 * n32 * n44 + n22 * n33 * n44;
    float t12 = n14 * n33 * n42 - n13 * n34 * n42 - n14 * n32 * n43 + n12 * n34 * n43 + n13 * n32 * n44 - n12 * n33 * n44;
    float t13 = n13 * n24 * n42 - n14 * n23 * n42 + n14 * n22 * n43 - n12 * n24 * n43 - n13 * n22 * n44 + n12 * n23 * n44;
    float t14 = n14 * n23 * n32 - n13 * n24 * n32 - n14 * n22 * n33 + n12 * n24 * n33 + n13 * n22 * n34 - n12 * n23 * n34;

    float det = n11 * t11 + n21 * t12 + n31 * t13 + n41 * t14;
    float idet = 1.0f / det;

    float4x4 ret;

    ret[0][0] = t11 * idet;
    ret[0][1] = (n24 * n33 * n41 - n23 * n34 * n41 - n24 * n31 * n43 + n21 * n34 * n43 + n23 * n31 * n44 - n21 * n33 * n44) * idet;
    ret[0][2] = (n22 * n34 * n41 - n24 * n32 * n41 + n24 * n31 * n42 - n21 * n34 * n42 - n22 * n31 * n44 + n21 * n32 * n44) * idet;
    ret[0][3] = (n23 * n32 * n41 - n22 * n33 * n41 - n23 * n31 * n42 + n21 * n33 * n42 + n22 * n31 * n43 - n21 * n32 * n43) * idet;

    ret[1][0] = t12 * idet;
    ret[1][1] = (n13 * n34 * n41 - n14 * n33 * n41 + n14 * n31 * n43 - n11 * n34 * n43 - n13 * n31 * n44 + n11 * n33 * n44) * idet;
    ret[1][2] = (n14 * n32 * n41 - n12 * n34 * n41 - n14 * n31 * n42 + n11 * n34 * n42 + n12 * n31 * n44 - n11 * n32 * n44) * idet;
    ret[1][3] = (n12 * n33 * n41 - n13 * n32 * n41 + n13 * n31 * n42 - n11 * n33 * n42 - n12 * n31 * n43 + n11 * n32 * n43) * idet;

    ret[2][0] = t13 * idet;
    ret[2][1] = (n14 * n23 * n41 - n13 * n24 * n41 - n14 * n21 * n43 + n11 * n24 * n43 + n13 * n21 * n44 - n11 * n23 * n44) * idet;
    ret[2][2] = (n12 * n24 * n41 - n14 * n22 * n41 + n14 * n21 * n42 - n11 * n24 * n42 - n12 * n21 * n44 + n11 * n22 * n44) * idet;
    ret[2][3] = (n13 * n22 * n41 - n12 * n23 * n41 - n13 * n21 * n42 + n11 * n23 * n42 + n12 * n21 * n43 - n11 * n22 * n43) * idet;

    ret[3][0] = t14 * idet;
    ret[3][1] = (n13 * n24 * n31 - n14 * n23 * n31 + n14 * n21 * n33 - n11 * n24 * n33 - n13 * n21 * n34 + n11 * n23 * n34) * idet;
    ret[3][2] = (n14 * n22 * n31 - n12 * n24 * n31 - n14 * n21 * n32 + n11 * n24 * n32 + n12 * n21 * n34 - n11 * n22 * n34) * idet;
    ret[3][3] = (n12 * n23 * n31 - n13 * n22 * n31 + n13 * n21 * n32 - n11 * n23 * n32 - n12 * n21 * n33 + n11 * n22 * n33) * idet;

    return ret;
}
#endif