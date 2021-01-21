#include "Lights.h"

static const float LUT_SIZE = 64.0;
static const float LUT_SCALE = (LUT_SIZE - 1.0) / LUT_SIZE;
static const float LUT_BIAS = 0.5 / LUT_SIZE;

SamplerState brdfSampler : register(s1);
Texture2D brdfLUT : register(t2, space1);
Texture2D LtcLUT : register(t3, space1);
Texture2D LtcLUT2 : register(t4, space1);
Texture2DArray prefilteredLTCTex : register(t5, space1);
SamplerState basicSampler : register(s0);



/* Get uv coordinates into LTC lookup texture */
float2 LtcCoords(float cosTheta, float roughness)
{
    const float theta = (cosTheta);
    float2 coords = float2(roughness, theta / (0.5 * 3.14159265f));

    /* Scale and bias coordinates, for correct filtered lookup */
    coords = coords * (LUT_SCALE) + LUT_BIAS;

    return coords;
}

float3 IntegrateEdgeVec(float3 v1, float3 v2)
{
    float x = dot(v1, v2);
    float y = abs(x);

    float a = 0.8543985 + (0.4965155 + 0.0145206 * y) * y;
    float b = 3.4175940 + (4.1616724 + y) * y;
    float v = a / b;

    float theta_sintheta = (x > 0.0) ? v : 0.5 * rsqrt(max(1.0 - x * x, 1e-7)) - v;

    return cross(v1, v2) * theta_sintheta;
}

float IntegrateEdge(float3 v1, float3 v2)
{
    return IntegrateEdgeVec(v1, v2).z;
}

void ClipQuadToHorizon(inout float3 L[5], out int n)
{
    // detect clipping config
    int config = 0;
    if (L[0].z > 0.0)
        config += 1;
    if (L[1].z > 0.0)
        config += 2;
    if (L[2].z > 0.0)
        config += 4;
    if (L[3].z > 0.0)
        config += 8;

    // clip
    n = 0;

    if (config == 0)
    {
        // clip all
    }
    else if (config == 1) // V1 clip V2 V3 V4
    {
        n = 3;
        L[1] = -L[1].z * L[0] + L[0].z * L[1];
        L[2] = -L[3].z * L[0] + L[0].z * L[3];
    }
    else if (config == 2) // V2 clip V1 V3 V4
    {
        n = 3;
        L[0] = -L[0].z * L[1] + L[1].z * L[0];
        L[2] = -L[2].z * L[1] + L[1].z * L[2];
    }
    else if (config == 3) // V1 V2 clip V3 V4
    {
        n = 4;
        L[2] = -L[2].z * L[1] + L[1].z * L[2];
        L[3] = -L[3].z * L[0] + L[0].z * L[3];
    }
    else if (config == 4) // V3 clip V1 V2 V4
    {
        n = 3;
        L[0] = -L[3].z * L[2] + L[2].z * L[3];
        L[1] = -L[1].z * L[2] + L[2].z * L[1];
    }
    else if (config == 5) // V1 V3 clip V2 V4) impossible
    {
        n = 0;
    }
    else if (config == 6) // V2 V3 clip V1 V4
    {
        n = 4;
        L[0] = -L[0].z * L[1] + L[1].z * L[0];
        L[3] = -L[3].z * L[2] + L[2].z * L[3];
    }
    else if (config == 7) // V1 V2 V3 clip V4
    {
        n = 5;
        L[4] = -L[3].z * L[0] + L[0].z * L[3];
        L[3] = -L[3].z * L[2] + L[2].z * L[3];
    }
    else if (config == 8) // V4 clip V1 V2 V3
    {
        n = 3;
        L[0] = -L[0].z * L[3] + L[3].z * L[0];
        L[1] = -L[2].z * L[3] + L[3].z * L[2];
        L[2] = L[3];
    }
    else if (config == 9) // V1 V4 clip V2 V3
    {
        n = 4;
        L[1] = -L[1].z * L[0] + L[0].z * L[1];
        L[2] = -L[2].z * L[3] + L[3].z * L[2];
    }
    else if (config == 10) // V2 V4 clip V1 V3) impossible
    {
        n = 0;
    }
    else if (config == 11) // V1 V2 V4 clip V3
    {
        n = 5;
        L[4] = L[3];
        L[3] = -L[2].z * L[3] + L[3].z * L[2];
        L[2] = -L[2].z * L[1] + L[1].z * L[2];
    }
    else if (config == 12) // V3 V4 clip V1 V2
    {
        n = 4;
        L[1] = -L[1].z * L[2] + L[2].z * L[1];
        L[0] = -L[0].z * L[3] + L[3].z * L[0];
    }
    else if (config == 13) // V1 V3 V4 clip V2
    {
        n = 5;
        L[4] = L[3];
        L[3] = L[2];
        L[2] = -L[1].z * L[2] + L[2].z * L[1];
        L[1] = -L[1].z * L[0] + L[0].z * L[1];
    }
    else if (config == 14) // V2 V3 V4 clip V1
    {
        n = 5;
        L[4] = -L[0].z * L[3] + L[3].z * L[0];
        L[0] = -L[0].z * L[1] + L[1].z * L[0];
    }
    else if (config == 15) // V1 V2 V3 V4
    {
        n = 4;
    }

    if (n == 3)
        L[3] = L[0];
    if (n == 4)
        L[4] = L[0];
}

float PolygonalClippedFormFactorToHorizonClippedSphereFormFactor(float3 F)
{
    float l = length(F);
    return max((l * l + F.z) / (l + 1), 0.0f);

}

struct Ray
{
    float3 origin;
    float3 dir;
};

struct AreaRect
{
    float halfx;
    float halfy;
    float3 dirx;
    float3 diry;
    float3 center;
    
    float4 plane;
};


bool RayPlaneIntersect(Ray ray, float4 plane, out float t)
{
    t = -dot(plane, float4(ray.origin, 1.0)) / dot(plane.xyz, ray.dir);
    return t > 0.0;
}

bool RayRectIntersect(Ray ray, AreaRect rect, out float t)
{
    bool intersect = RayPlaneIntersect(ray, rect.plane, t);
    if (intersect)
    {
        float3 pos = ray.origin + ray.dir * t;
        float3 lpos = pos - rect.center;

        float x = dot(lpos, rect.dirx);
        float y = dot(lpos, rect.diry);

        if (abs(x) > rect.halfx || abs(y) > rect.halfy)
            intersect = false;
    }

    return intersect;
}

float2 RectUVs(float3 pos, AreaRect rect)
{
    float3 lpos = pos - rect.center;

    float x = dot(lpos, rect.dirx);
    float y = dot(lpos, rect.diry);

    return float2(
        0.5 * x / rect.halfx + 0.5,
        0.5 * y / rect.halfy + 0.5);
}

float3 FetchColorTexture(float2 uv, float lod)
{
    return prefilteredLTCTex.Sample(basicSampler, float3(uv, lod)).rgb;
}

float3 GetPrefilteredTextureColor(float3 p1, float3 p2, float3 p3, float3 p4, float3 dir)
{
    float3 V1 = p2 - p1;
    float3 V2 = p4 - p1;
   
    float3 planeOrtho = cross(V1, V2);
    float planeAreaSquared = dot(planeOrtho, planeOrtho);

    Ray ray;
    ray.origin = float3(0, 0, 0);
    ray.dir = dir;
    float4 plane = float4(planeOrtho, -dot(planeOrtho, p1));
    float planeDist;
    RayPlaneIntersect(ray, plane, planeDist);
 
    float3 P = planeDist * ray.dir - p1;
    
    P.x += 3;
    P.y -= 3;
 
    // find tex coords of P
    float dot_V1_V2 = dot(V1, V2);
    float inv_dot_V1_V1 = 1.0 / dot(V1, V1);
    float3 V2_ = V2 - V1 * dot_V1_V2 * inv_dot_V1_V1;
    float2 Puv;
    Puv.y = dot(V2_, P) / dot(V2_, V2_);
    Puv.x = dot(V1, P) * inv_dot_V1_V1 - dot_V1_V2 * inv_dot_V1_V1 * Puv.y;
    Puv *= 0.333f;
    
    // LOD
    float d = abs(planeDist) / pow(planeAreaSquared, 0.25);
    
    float lod = log(2048.0 * d) / log(3.0);
    lod = min(lod, 11.0);
    
    float lodA = floor(lod);
    float lodB = ceil(lod);
    float t = lod - lodA;
    
    float3 a = FetchColorTexture(Puv, lodA);
    float3 b = FetchColorTexture(Puv, lodB);

    return saturate(lerp(a, b, t));
}

float3 LTC_Evaluate(
    float3 N, float3 V, float3 P, float3x3 Minv, float3 points[4], float4 ltc2, bool twoSided, float width, float height, float3 pos, float3x3 TBN, Area rect)
{
    // construct orthonormal basis around N
    float3 T1, T2;
    T1 = normalize(V - N * dot(V, N));
    T2 = cross(N, T1);

    // rotate area light in (T1, T2, N) basis
    Minv = mul(Minv, (float3x3(T1, T2, N)));

    // polygon (allocate 5 vertices for clipping)
    float3 L[5];
    L[0] = mul(Minv, points[0] - P);
    L[1] = mul(Minv, points[1] - P);
    L[2] = mul(Minv, points[2] - P);
    L[3] = mul(Minv, points[3] - P);

    // integrate
    float sum = 0.0;
    float3 color = float3(1, 1, 1);
    if (true)
    {
        float3 dir = points[0].xyz - P;
        float3 lightNormal = cross(points[1] - points[0], points[3] - points[0]);
        bool behind = (dot(dir, lightNormal) < 0.0);
   
        L[0] = normalize(L[0]);
        L[1] = normalize(L[1]);
        L[2] = normalize(L[2]);
        L[3] = normalize(L[3]);
   
        float3 vsum = float3(0.0, 0, 0);
   
        vsum += IntegrateEdgeVec(L[0], L[1]);
        vsum += IntegrateEdgeVec(L[1], L[2]);
        vsum += IntegrateEdgeVec(L[2], L[3]);
        vsum += IntegrateEdgeVec(L[3], L[0]);
        
        float scale = PolygonalClippedFormFactorToHorizonClippedSphereFormFactor(vsum);
        
        float len = length(vsum);
        float3 direction = vsum / len;
        
        color = GetPrefilteredTextureColor(L[0], L[1], L[2], L[3], direction);
        
        sum = scale;
        if (behind && !twoSided)
            sum = 0.0;
    }
    else
    {
        int n;
        ClipQuadToHorizon(L, n);

        if (n == 0)
            return float3(0, 0, 0);
        // project onto sphere
        L[0] = normalize(L[0]);
        L[1] = normalize(L[1]);
        L[2] = normalize(L[2]);
        L[3] = normalize(L[3]);
        L[4] = normalize(L[4]);

        // integrate
        sum += IntegrateEdge(L[0], L[1]);
        sum += IntegrateEdge(L[1], L[2]);
        sum += IntegrateEdge(L[2], L[3]);
        if (n >= 4)
            sum += IntegrateEdge(L[3], L[4]);
        if (n == 5)
            sum += IntegrateEdge(L[4], L[0]);

        sum = twoSided ? abs(sum) : max(0.0, sum);
    }

    float3 Lo_i = float3(sum, sum, sum)*color;

    return (Lo_i / 2 * 3.141592);
}

// http://momentsingraphics.de/?p=105
//function to solve a cubic function
float3 SolveCubic(float4 Coefficient)
{
    // Normalize the polynomial
    Coefficient.xyz /= Coefficient.w;
    // Divide middle coefficients by three
    Coefficient.yz /= 3.0;

    float A = Coefficient.w;
    float B = Coefficient.z;
    float C = Coefficient.y;
    float D = Coefficient.x;

    // Compute the Hessian and the discriminant
    float3 Delta = float3(
        -Coefficient.z * Coefficient.z + Coefficient.y,
        -Coefficient.y * Coefficient.z + Coefficient.x,
        dot(float2(Coefficient.z, -Coefficient.y), Coefficient.xy)
    );

    float Discriminant = dot(float2(4.0 * Delta.x, -Delta.y), Delta.zy);

    float3 RootsA, RootsD;

    float2 xlc, xsc;

    // Algorithm A
    {
        float A_a = 1.0;
        float C_a = Delta.x;
        float D_a = -2.0 * B * Delta.x + Delta.y;

        // Take the cubic root of a normalized complex number
        float Theta = atan2(sqrt(Discriminant), -D_a) / 3.0;

        float x_1a = 2.0 * sqrt(-C_a) * cos(Theta);
        float x_3a = 2.0 * sqrt(-C_a) * cos(Theta + (2.0 / 3.0) * 3.14159265);

        float xl;
        if ((x_1a + x_3a) > 2.0 * B)
            xl = x_1a;
        else
            xl = x_3a;

        xlc = float2(xl - B, A);
    }

    // Algorithm D
    {
        float A_d = D;
        float C_d = Delta.z;
        float D_d = -D * Delta.y + 2.0 * C * Delta.z;

        // Take the cubic root of a normalized complex number
        float Theta = atan2(D * sqrt(Discriminant), -D_d) / 3.0;

        float x_1d = 2.0 * sqrt(-C_d) * cos(Theta);
        float x_3d = 2.0 * sqrt(-C_d) * cos(Theta + (2.0 / 3.0) * 3.14159265);

        float xs;
        if (x_1d + x_3d < 2.0 * C)
            xs = x_1d;
        else
            xs = x_3d;

        xsc = float2(-D, xs + C);
    }

    float E = xlc.y * xsc.y;
    float F = -xlc.x * xsc.y - xlc.y * xsc.x;
    float G = xlc.x * xsc.x;

    float2 xmc = float2(C * F - B * G, -B * F + C * E);

    float3 Root = float3(xsc.x / xsc.y, xmc.x / xmc.y, xlc.x / xlc.y);

    if (Root.x < Root.y && Root.x < Root.z)
        Root.xyz = Root.yxz;
    else if (Root.z < Root.x && Root.z < Root.y)
        Root.xyz = Root.xzy;

    return Root;
}


float3 LTC_EvaluateDisk(
    float3 N, float3 V, float3 P, float3x3 Minv, float3 points[4], float4 ltc2, bool twoSided)
{
    // construct orthonormal basis around N
    float3 T1, T2;
    T1 = normalize(V - N * dot(V, N));
    T2 = cross(N, T1);

    // rotate area light in (T1, T2, N) basis
    float3x3 R = (float3x3(T1, T2, N));

    // polygon (allocate 5 vertices for clipping)
    float3 L_0[3];
    L_0[0] = mul(R, points[0] - P);
    L_0[1] = mul(R, points[1] - P);
    L_0[2] = mul(R, points[2] - P);
    
       // init ellipse
   float3 C = 0.5 *  (L_0[0] + L_0[2]);
   float3 V1 = 0.5 * (L_0[1] - L_0[2]);
   float3 V2 = 0.5 * (L_0[1] - L_0[0]);

    C =  mul(Minv, C);
    V1 = mul(Minv, V1);
    V2 = mul(Minv, V2);
    
    if (!twoSided && dot(cross(V1, V2), C) < 0.0)
        return float3(0.0, 0, 0);

    // compute eigenvectors of ellipse
    float a, b;
    float d11 = dot(V1, V1);
    float d22 = dot(V2, V2);
    float d12 = dot(V1, V2);
    if (abs(d12) / sqrt(d11 * d22) > 0.0001)
    {
        float tr = d11 + d22;
        float det = -d12 * d12 + d11 * d22;

        // use sqrt matrix to solve for eigenvalues
        det = sqrt(det);
        float u = 0.5 * sqrt(tr - 2.0 * det);
        float v = 0.5 * sqrt(tr + 2.0 * det);
        float e_max = (u + v) * (u + v);
        float e_min = (u - v) * (u - v);

        float3 V1_, V2_;

        if (d11 > d22)
        {
            V1_ = d12 * V1 + (e_max - d11) * V2;
            V2_ = d12 * V1 + (e_min - d11) * V2;
        }
        else
        {
            V1_ = d12 * V2 + (e_max - d22) * V1;
            V2_ = d12 * V2 + (e_min - d22) * V1;
        }

        a = 1.0 / e_max;
        b = 1.0 / e_min;
        V1 = normalize(V1_);
        V2 = normalize(V2_);
    }
    else
    {
        a = 1.0 / dot(V1, V1);
        b = 1.0 / dot(V2, V2);
        V1 *= sqrt(a);
        V2 *= sqrt(b);
    }
    
    float3 V3 = cross(V1, V2);
    if (dot(C, V3) < 0.0)
        V3 *= -1.0;

    float L = dot(V3, C);
    float x0 = dot(V1, C) / L;
    float y0 = dot(V2, C) / L;

    float E1 = rsqrt(a);
    float E2 = rsqrt(b);

    a *= L * L;
    b *= L * L;

    float c0 = a * b;
    float c1 = a * b * (1.0 + x0 * x0 + y0 * y0) - a - b;
    float c2 = 1.0 - a * (1.0 + x0 * x0) - b * (1.0 + y0 * y0);
    float c3 = 1.0;

    float3 roots = SolveCubic(float4(c0, c1, c2, c3));
    float e1 = roots.x;
    float e2 = roots.y;
    float e3 = roots.z;

    float3 avgDir = float3(a * x0 / (a - e2), b * y0 / (b - e2), 1.0);

    float3x3 rotate = transpose(float3x3(V1, V2, V3));

    avgDir = mul(rotate, avgDir);
    avgDir = normalize(avgDir);

    float L1 = sqrt(-e2 / e3);
    float L2 = sqrt(-e2 / e1);

    float formFactor = L1 * L2 * rsqrt((1.0 + L1 * L1) * (1.0 + L2 * L2));
   
    // use tabulated horizon-clipped sphere
    float2 uv = float2(avgDir.z * 0.5 + 0.5, formFactor);
    uv = uv * LUT_SCALE + LUT_BIAS;
    float scale = LtcLUT2.Sample(basicSampler, uv).w;
    
    float spec = formFactor * scale;
    spec = saturate(spec);
    
    return float3(spec,spec,spec);

}

float3 RotationY(float3 v, float a)
{
    float3 r;
    r.x = v.x * cos(a) + v.z * sin(a);
    r.y = v.y;
    r.z = -v.x * sin(a) + v.z * cos(a);
    return r;
}

float3 RotationZ(float3 v, float a)
{
    float3 r;
    r.x = v.x * cos(a) - v.y * sin(a);
    r.y = v.x * sin(a) + v.y * cos(a);
    r.z = v.z;
    return r;
}

float3 RotationX(float3 v, float a)
{
    float3 r;
    r.x = v.x;
    r.y = cos(a) * v.y - v.z * sin(a);
    r.z = sin(a) * v.y + cos(a) * v.z;
    
    return r;
}

float3 RotationYZ(float3 v, float ay, float az)
{
    return RotationZ(RotationY(v, ay), az);
}

float3 RotationXYZ(float3 v, float ax, float ay, float az)
{
    return RotationX(RotationYZ(v, ay, az), ax);
}


void InitRectPoints(Area rect, out float3 points[4])
{
    float3 ex = rect.halfx * rect.dirx;
    float3 ey = rect.halfy * rect.diry;

    points[0] = rect.center - ex - ey;
    points[1] = rect.center + ex - ey;
    points[2] = rect.center + ex + ey;
    points[3] = rect.center - ex + ey;
}

void InitRect(out Area rect, Light light)
{
    rect.dirx = RotationXYZ(float3(1, 0, 0), light.rectLight.rotX * 2 * 3.14159265f, light.rectLight.rotY * 2 * 3.14159265f, light.rectLight.rotZ * 2 * 3.14159265f);
    rect.diry = RotationXYZ(float3(0, 1, 0), light.rectLight.rotX * 2 * 3.14159265f, light.rectLight.rotY * 2 * 3.14159265f, light.rectLight.rotZ * 2 * 3.14159265f);
    
    rect.center = light.position;
    rect.halfx = 0.5 * light.rectLight.width;
    rect.halfy = 0.5 * light.rectLight.height;

    float3 rectNormal = cross(rect.dirx, rect.diry);
}

void InitDisk(out Area disk, Light light)
{
    disk.dirx = RotationXYZ(float3(1, 0, 0), light.rectLight.rotX * 2 * 3.14159265f, light.rectLight.rotY * 2 * 3.14159265f, light.rectLight.rotZ * 2 * 3.14159265f);
    disk.diry = RotationXYZ(float3(0, 1, 0), light.rectLight.rotX * 2 * 3.14159265f, light.rectLight.rotY * 2 * 3.14159265f, light.rectLight.rotZ * 2 * 3.14159265f);
    disk.center = light.position;
    disk.halfx = 0.5 * light.rectLight.width;
    disk.halfy = 0.5 * light.rectLight.height;
}

void InitDiskPoints(Area disk, out float3 points[4])
{
    float3 ex = disk.halfx * disk.dirx;
    float3 ey = disk.halfy * disk.diry;

    points[0] = disk.center - ex - ey;
    points[1] = disk.center + ex - ey;
    points[2] = disk.center + ex + ey;
    points[3] = disk.center - ex + ey;
}

