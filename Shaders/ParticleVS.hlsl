#include "Common.hlsl"
cbuffer externalData: register(b0)
{
	matrix view;
	matrix projection;

	int startIndex;

	float3 acceleration;

	float4 startColor;
	float4 endColor;

	float startSize;
	float endSize;

	float lifetime;
	float currentTime;
};

struct Particle
{
	float spawnTime;
	float3 startPosition;

	float rotationStart;
	float3 startVelocity;

	float rotationEnd;
	float3 padding;
};

struct VertexToPixel
{
	float4 position: SV_POSITION;
	float2 uv : TEXCOORD;
	float4 color: COLOR;
};

StructuredBuffer<Particle> ParticleData: register(t0);

VertexToPixel main(uint id: SV_VertexID)
{
	VertexToPixel output;

	uint particleID = id / 4; //every group of 4 vertex is a particle
	uint cornerID = id % 4;

	Particle p = ParticleData.Load(particleID + startIndex);

	float t = currentTime - p.spawnTime;
	float percent = t / lifetime; //percent to lerp with

	float3 pos = 0.5 * t * t * acceleration + t * p.startVelocity + p.startPosition;
	float4 color = lerp(startColor, endColor, percent);
	float size = lerp(startSize, endSize, percent);
	float rotation = lerp(p.rotationStart, p.rotationEnd, percent);

	float2 offsets[4];
	offsets[0] = float2(-1.0f, 1.0f); //top left
	offsets[1] = float2(1.0f, 1.0f); //top right
	offsets[2] = float2(1.0f, -1.0f); //back right
	offsets[3] = float2(-1.0f, -1.0f); //back left

	float c, s;
	sincos(rotation, s, c);

	float2x2 rot =
	{
		c,s
		,-s,c
	};

	float2 rotatedOffset = mul(offsets[cornerID], rot);

	pos += float3(view._11, view._21, view._31) * rotatedOffset.y * size;
	pos += float3(view._12, view._22, view._32) * rotatedOffset.x * size;

	matrix viewProj = mul(view, projection);

	output.position = mul(float4(pos, 1.0f), viewProj);

	float2 uvs[4];
	uvs[0] = float2(0, 0);  // TL
	uvs[1] = float2(1, 0);  // TR
	uvs[2] = float2(1, 1);  // BR
	uvs[3] = float2(0, 1);  // BL

	// Pass uv through
	output.uv = saturate(uvs[cornerID]);
	output.color = color;

	return output;
}