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
	float4 position		: SV_POSITION;
	float3 noisePos		: TEXCOORD;
	float3 worldPos		: TEXCOORD1;
};


cbuffer VolumeData :register(b0)
{
	matrix model;
	matrix inverseModel;
	matrix view;
	matrix proj;
	float3 cameraPosition;
	float focalLength;
	float time;
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

float3 GetUV(float3 p)
{
	// float3 local = localize(p);
	//p.y *= -1;
	float3 local = p + 0.5;
	return local;
}

float4 SampleVolume(float3 uv, float3 p, Texture3D flame, SamplerState basicSampler,matrix world)
{
	float4 v = flame.Sample(basicSampler,uv).rgba*0.9;

	float3 axis = mul(float4(p,0), world).xyz;
	axis = GetUV(axis);
	float min = step(0, axis.x) * step(0, axis.y) * step(0, axis.z);
	float max = step(axis.x, 1) * step(axis.y, 1) * step(axis.z, 1);

	return v;
}

//Texture3D volume: register(t0); 
Texture3D flame: register(t0);
SamplerState basicSampler: register(s0);

float rand(in float2 uv)
{
	float2 noise = (frac(sin(dot(uv, float2(12.9898, 78.233) * 2.0)) * 43758.5453));
	return abs(noise.x + noise.y) * 0.5;
}

float Turbulence4(float3 p)
{
	float sum = rand(p.xz) * 0.5 + rand(p.xy * 2) * 0.25 * rand(p.xz * 4) * 0.125 * rand(p.xz * 8) * 0.0625;
	return sum;
}

float4 Flame(float3 P, VertexToPixel input)
{
	P = P * float3(1,-1,1) + float3(0,0,0);
	// calculate radial distance in XZ plane
	float2 uv;
	uv.x = length(P.xz);
	uv.y = P.y+Turbulence4(input.noisePos) * 1.f;
	return flame.Sample(basicSampler, uv.xxx);
}

float4 main(VertexToPixel input) : SV_TARGET
{

	float3 rayDirection;
	rayDirection.xy = input.position.xy / input.position.w;
	rayDirection.x *= 2 - 1;
	rayDirection.y = -rayDirection.y * 2 + 1;
	rayDirection.z = -focalLength;

	matrix modelView = mul(model, view);
	rayDirection = mul(float4(rayDirection, 0), view).xyz;

	float t0, t1;

	Ray ray;
	ray.origin = cameraPosition;
	float3 dir = input.worldPos - cameraPosition;
	ray.direction = normalize(mul(float4(dir,0),inverseModel)).xyz;

	AABB boundingBox;
	boundingBox.min = float3(-0.5,-0.5,-0.5);
	boundingBox.max = float3(0.5,0.5,0.5);

	bool hit = RayBoxIntersection(ray, boundingBox, t0, t1);

	//if (!hit) discard;

	if (t0 < 0) t0 = 0;

	float3 rayStart = (ray.origin+ray.direction*t0);
	float3 rayStop = (ray.origin + ray.direction * t1);

	float3 rayEX = rayStop - rayStart;
	float rayLength = abs(t1-t0);
	//float3 stepVector = 0.01f * rayEX / rayLength;
	//float3 position = rayStart;
	float stepSize = rayLength / 100.f;
	float3 step = normalize(rayEX) * stepSize;

	//float3 step = (rayStart - rayStop) / 99;
	float maximumIntensity = 0.0;

	float4 c = 0;

	//float3 step = (rayStart - rayStop) / (100 - 1);
	float3 P = rayStart;
	P.y *= -1;
	/**/for (int i = 0; i < 100; i++) 
	{
		
			float3 uv = GetUV(P);
			float4 v = SampleVolume(uv, P,flame,basicSampler,model);
			float4 src = float4(v);
			src.a *= 0.5;
			src.rgb *= src.a;

			// blend
			c = (1.0 - c.a) * src + c;
			P += step;

			if (c.a > 0.95) break;
	}

	/*for (int i = 0; i < 100; i++)
	{
		float4 s = Flame(P, input);
		c += s.a * s + (1 - s.a) * c;
		P += step;
	}
	c/=100.f;*/
	return saturate(c);


}