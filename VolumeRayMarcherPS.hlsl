#include "Common.hlsl"

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
	noperspective float3 noisePos		: TEXCOORD;
	float3 worldPos		: TEXCOORD1;
};




cbuffer VolumeData :register(b0)
{
	matrix model;
	matrix inverseModel;
	matrix view;
	matrix viewInv;
	matrix proj;
	float3 cameraPosition;
	float focalLength;
	float time;
};
// this noise lookup inspired by iq's 3D noise lookup in his clouds shader

float hash(float n)
{
    return frac(sin(n) * 43758.5453);
}

float noise(float3 x)
{
    // The noise function returns a value in the range -1.0f -> 1.0f

    float3 p = floor(x);
    float3 f = frac(x);

    f = f * f * (3.0 - 2.0 * f);
    float n = p.x + p.y * 57.0 + 113.0 * p.z;

    return lerp(lerp(lerp(hash(n + 0.0), hash(n + 1.0), f.x),
                   lerp(hash(n + 57.0), hash(n + 58.0), f.x), f.y),
               lerp(lerp(hash(n + 113.0), hash(n + 114.0), f.x),
                   lerp(hash(n + 170.0), hash(n + 171.0), f.x), f.y), f.z);
}


float fbm(float3 p)
{
    return abs(
           noise(p * 1.) * .6) +
           noise(p * 2.) * .3 +
           noise(p * 4.) * .25 +
           noise(p * 8.) * .125;
}
// makes a sphere
// x - length(p) is inverse distance function 
// points with length less than x get positive values, outside of x radius values become negative
// high enough values from the FBM can outweigh the negative distance values
float scene(float3 p)
{
    return .5 - length(p) * 0.05 + fbm(p * .0321);
}


float3 Translate(float3 pos, float3 translate)
{
    return pos -= translate;
}

// Taken from https://iquilezles.org/www/articles/distfunctions/distfunctions.htm
float sdSphere(float3 p, float3 origin, float s)
{
    p = Translate(p, origin);
    return length(p) - s;
}

// Taken from https://iquilezles.org/www/articles/distfunctions/distfunctions.htm
float sdPlane(float3 p)
{
    return p.y;
}

float sdSmoothUnion(float d1, float d2, float k)
{
    float h = clamp(0.5 + 0.5 * (d2 - d1) / k, 0.0, 1.0);
    return lerp(d2, d1, h) - k * h * (1.0 - h);
}

float QueryVolumetricDistanceField(in float3 pos)
{
    float3 fbmCoord = (pos + 2.0 * float3(1, 0.0, 1)) / 1.5f;
    float sdfValue = sdSphere(pos, float3(-8.0, 2.0 + 20.0 * sin(1), -1), 5.6);
    sdfValue = sdSmoothUnion(sdfValue, sdSphere(pos, float3(8.0, 8.0 + 12.0 * cos(1), 3), 5.6), 3.0f);
    sdfValue = sdSmoothUnion(sdfValue, sdSphere(pos, float3(5.0 * sin(1), 3.0, 0), 8.0), 3.0) + 7.0 * fbm(fbmCoord / 3.2);
    sdfValue = sdSmoothUnion(sdfValue, sdPlane(pos + float3(0, 0.4, 0)), 22.0);
    return sdfValue;
}

float SphereHit(float3 p)
{
	return distance(p, float3(0,0,0)) - 0.5;
}

float4 RaymarchHit(float3 position, float3 direction)
{
	for (int i = 0; i < 64; i++)
	{
        float distance = QueryVolumetricDistanceField(position);
		if (distance<0.01)
			return float4(1, 0, 0, 1);

		position += direction * distance;
	}

	return float4(0, 0, 0, 0);
}




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
	float4 v = flame.Sample(basicSampler,uv).rgba;

	float3 axis = mul(float4(p,0), world).xyz;
	axis = GetUV(axis);
	axis = saturate(axis);
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
	float t0, t1;

	float3 worldPosition = input.worldPos;

	float3 viewDirection = normalize(input.worldPos - cameraPosition);

	return RaymarchHit(worldPosition, viewDirection);
	//if (RaymarchHit(worldPosition, viewDirection)) return float4(1, 0, 0, 1);
	//else return float4(0, 0, 0, 0);

	Ray ray;
	ray.origin = cameraPosition;//mul(float4(0,0,0,1),viewInv).xyz;
	float3 dir = input.worldPos - cameraPosition;
	ray.direction.xy = ((input.position.xy * 2.0) - 1.0) * float2(WIDTH, HEIGHT);
	ray.direction.y *= -1;
	ray.direction.z = focalLength;
	ray.direction = mul(ray.direction, (float3x3)viewInv);
	ray.direction = normalize(ray.direction);
	ray.direction = normalize(mul(float4(dir,0),inverseModel)).xyz;

	AABB boundingBox;
	boundingBox.min = float3(-0.5,-0.5,-0.5);
	boundingBox.max = float3(0.5,0.5,0.5);

	bool hit = RayBoxIntersection(ray, boundingBox, t0, t1);

	if (!hit) discard;

	if (t0 < 0) t0 = 0;

	float3 rayStart = (ray.origin+ray.direction*t0);
	float3 rayStop = (ray.origin + ray.direction * t1);
	float dist = abs(t1 - t0);
	float stepSize = dist / 64;
	float3 ds = normalize(rayStop - rayStart) * stepSize;
	//rayStart = rayStart * 0.5 + 0.5;
	//rayStop = rayStop * 0.5 + 0.5;

	float4 c = 0;

	float3 rayEX = rayStop - rayStart;
	float rayLength = distance(rayStart,rayStop);
	//float3 stepVector = 0.01f * rayEX / rayLength;
	//float3 position = rayStart;
	//float stepSize = rayLength / 100.f;
	float3 step = normalize(rayEX) * 0.01;

	//float3 step = (rayStop- rayStart) / 99;
	float maximumIntensity = 0.0;



	//float3 step = (rayStart - rayStop) / (100 - 1);
	float3 P = rayStart;
	//P.y *= -1;
	[loop]
	/**/for (int i = 0; i < 64; i++) 
	{
		
			float3 uv = GetUV(P);
			uv = saturate(uv);
			uv /= 2;
			float4 v = SampleVolume(uv, P,flame,basicSampler,model);
			float4 src = float4(v);
			src.a *= 0.5;
			src.rgb *= src.a;

			// blend
			c = (1.0 - c.a) * src + c;
			P += ds;

			if (c.a > 0.95) break;
	}

	/*for (int i = 0; i < 100; i++)
	{
		float4 s = flame.Sample(basicSampler,P);
		c = s.a * s + (1 - s.a) * c;
		P += step;
	}*/
	//c/=100.f;
	//c *= 10;
	return saturate(c);


}