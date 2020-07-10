#include "Common.hlsl"

static const float M_PI = 3.14159265f;

//bool ShootShadowRays(float3 origin, float3 direction, float minT, float maxT)
//{
//	//initialize the hit payload
//	ShadowHitInfo shadowPayload;
//	shadowPayload.isHit = false;
//
//	/**/RayDesc ray;
//	ray.Origin = origin;
//	ray.Direction = direction;
//	ray.TMin = minT;
//	ray.TMax = 100000;
//
//	TraceRay(SceneBVH, RAY_FLAG_NONE, 0xFF, 1, 2, 1, ray, shadowPayload);
//
//	return isHit;
//}
//
//
//float3 DiffuseShade(float3 pos, float3 norm, float3 albedo, inout uint seed, DirectionalLight light1)
//{
//
//	float3 L = -light1.direction;
//
//	float NdotL = saturate(dot(norm, L));
//
//	bool isHit = ShootShadowRay(pos, L, 1.0e-4f, 10000000);
//
//	float factor = isHit ? 0.3 : 1.0;
//	float3 rayColor = light1.diffuse * factor;
//
//	return (NdotL * rayColor * (albedo / PI));
//}

// Utility function to get a vector perpendicular to an input vector 
//    (from "Efficient Construction of Perpendicular Vectors Without Branching")
float3 getPerpendicularVector(float3 u)
{
	float3 a = abs(u);
	uint xm = ((a.x - a.y) < 0 && (a.x - a.z) < 0) ? 1 : 0;
	uint ym = (a.y - a.z) < 0 ? (1 ^ xm) : 0;
	uint zm = 1 ^ (xm | ym);
	return cross(u, float3(xm, ym, zm));
}

// A work-around function because some DXR drivers seem to have broken atan2() implementations
float atan2_WAR(float y, float x)
{
	if (x > 0.f)
		return atan(y / x);
	else if (x < 0.f && y >= 0.f)
		return atan(y / x) + M_PI;
	else if (x < 0.f && y < 0.f)
		return atan(y / x) - M_PI;
	else if (x == 0.f && y > 0.f)
		return M_PI / 2.f;
	else if (x == 0.f && y < 0.f)
		return -M_PI / 2.f;
	return 0.f; // x==0 && y==0 (undefined)
}

// Convert our world space direction to a (u,v) coord in a latitude-longitude spherical map
float2 wsVectorToLatLong(float3 dir)
{
	float3 p = normalize(dir);

	// atan2_WAR is a work-around due to an apparent compiler bug in atan2
	float u = (1.f + atan2_WAR(p.x, -p.z) * M_PI) * 0.5f;
	float v = acos(p.y) * M_PI;
	return float2(u, v);
}

// Generates a seed for a random number generator from 2 inputs plus a backoff
uint initRand(uint val0, uint val1, uint backoff = 16)
{
	uint v0 = val0, v1 = val1, s0 = 0;

	[unroll]
	for (uint n = 0; n < backoff; n++)
	{
		s0 += 0x9e3779b9;
		v0 += ((v1 << 4) + 0xa341316c) ^ (v1 + s0) ^ ((v1 >> 5) + 0xc8013ea4);
		v1 += ((v0 << 4) + 0xad90777d) ^ (v0 + s0) ^ ((v0 >> 5) + 0x7e95761e);
	}
	return v0;
}

// Takes our seed, updates it, and returns a pseudorandom float in [0..1]
float nextRand(inout uint s)
{
	s = (1664525u * s + 1013904223u);
	return float(s & 0x00FFFFFF) / float(0x01000000);
}

// Get a cosine-weighted random vector centered around a specified normal direction.
float3 getCosHemisphereSample(inout uint randSeed, float3 hitNorm)
{
	// Get 2 random numbers to select our sample with
	float2 randVal = float2(nextRand(randSeed), nextRand(randSeed));

	// Cosine weighted hemisphere sample from RNG
	float3 bitangent = getPerpendicularVector(hitNorm);
	float3 tangent = cross(bitangent, hitNorm);
	float r = sqrt(randVal.x);
	float phi = 2.0f * 3.14159265f * randVal.y;

	// Get our cosine-weighted hemisphere lobe sample direction
	return tangent * (r * cos(phi).x) + bitangent * (r * sin(phi)) + hitNorm.xyz * sqrt(max(0.0, 1.0f - randVal.x));
}

float3 ConvertFromObjectToWorld(float3 vec)
{
	return mul(vec, ObjectToWorld3x4());
}
