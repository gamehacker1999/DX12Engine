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