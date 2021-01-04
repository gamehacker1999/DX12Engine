static const float PI = 3.14159265359f;
static const float TWO_PI = PI * 2.0f;
static const float PI_OVER_2 = PI / 2.0f;


// Converts a direction in space to a UV coord for an
// equirectorial (or latitude/longitude) map
float2 DirectionToLatLongUV(float3 dir)
{
	// Calculate polar coords
	dir = normalize(dir);
	float theta = acos(dir.y);
	float phi = atan2(dir.z, -dir.x);

	// Normalize
	return float2(
		(PI + phi) / TWO_PI,
		theta / PI);
}

float4 ConvertToYCoCg(float4 rgba)
{
    return float4(
		dot(rgba.rgb, float3(0.25f, 0.50f, 0.25f)),
		dot(rgba.rgb, float3(0.50f, 0.00f, -0.50f)),
		dot(rgba.rgb, float3(-0.25f, 0.50f, -0.25f)),
		rgba.a);
}

float4 ConvertToRGBA(float4 yCoCg)
{
    return float4(
		yCoCg.x + yCoCg.y - yCoCg.z,
		yCoCg.x + yCoCg.z,
		yCoCg.x - yCoCg.y - yCoCg.z,
		yCoCg.a);
}

float IntersectAABB(float3 origin, float3 direction, float3 extents)
{
    float3 reciprocal = rcp(direction);
    float3 minimum = (extents - origin) * reciprocal;
    float3 maximum = (-extents - origin) * reciprocal;
	
    return max(max(min(minimum.x, maximum.x), min(minimum.y, maximum.y)), min(minimum.z, maximum.z));
}