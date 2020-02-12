struct Ray
{
	float3 origin;
	float3 direction;
};

struct AABB
{
	float3 min;
	float3 max;
};

struct VertexToPixel
{
	float4 position: SV_POSITION;
	float4 color: COLOR;
	float3 normal:  NORMAL;
	float3 tangent: TANGENT;
	float3 worldPosition: POSITION;
	float2 uv: TEXCOORD;

};																																											

cbuffer VolumeData :register(b0)
{
	matrix view;
	matrix model;
	float3 cameraPosition;
	float focalLength;
};

bool RayBoxIntersection(Ray ray, AABB box, out float t0, out float t1)
{
	// compute intersection of ray with all six bbox planes
	float3 invRay = 1.0 / ray.direction;
	float3 ttop = invRay * (box.min - ray.origin);
	float3 tbottom = invRay * (box.max - ray.origin);

	float3 minimum = min(ttop,tbottom);
	float3 maximum = max(ttop, tbottom);

	// find the largest tmin and the smallest tmax
	float2 t = max(minimum.xx, minimum.yz);
	t0 = max(0, max(t.x, t.y));

	t = min(maximum.xx, maximum.yz);
	t1 = min(t.x, t.y);

	bool hit;
	if (t0 > t1) hit = false;
	else hit = true;

	return hit;
}

//Texture3D volume: register(t0); 
Texture2D flame: register(t1);
SamplerState basicSampler: register(s0);

float4 Flame(float3 P)
{
	//P = P * flameScale + flameTrans;
	// calculate radial distance in XZ plane
	float2 uv;
	uv.x = length(P.xz);
	uv.y = P.y;// +turbulence4(noiseSampler, noisePos) * noiseStrength;
	return flame.Sample(basicSampler, P.xy);
}

float4 main(VertexToPixel input) : SV_TARGET
{

	float3 rayDirection;
	rayDirection.xy = input.position.xy / input.position.w;
	rayDirection.x *= 2 - 1;
	rayDirection.y = -rayDirection.y * 2 + 1;
	rayDirection.z = -1;

	matrix modelView = mul(model, view);
	rayDirection = mul(float4(rayDirection, 0), modelView).xyz;

	float t0, t1;

	Ray ray;
	ray.origin = cameraPosition;
	ray.direction = rayDirection;

	AABB boundingBox;
	boundingBox.min = float3(-1, -1, -1);
	boundingBox.max = float3(1, 1, 1);

	bool hit = RayBoxIntersection(ray, boundingBox, t0, t1);

	if (!hit) discard;

	if (t0 < 0) t0 = 0;

	float3 rayStart = (cameraPosition + rayDirection * t0);
	float3 rayStop = (cameraPosition + rayDirection * t1);

	float3 rayEX = rayStop - rayStart;
	float rayLength = length(rayEX);
	float3 stepVector = 0.01f * rayEX / rayLength;
	float3 position = rayStart;

	float maximumIntensity = 0.0;

	float4 c = 0;

	float3 step = (rayStart - rayStop) / (100 - 1);
	float3 P = rayStop;
	for (int i = 0; i < 100; i++) 
	{
		float4 s = Flame(P);
		c = s.a * s + (1.0 - s.a) * c;
		P += step;
	}
	c /= 100;
	return c;


}