#include "Common.hlsl"

// Constant Buffer
cbuffer externalData : register(b0)
{
	matrix world;
	matrix view;
	float4 clipDistance;
	/*float amplitude;
	float steepness;
	float wavelength;
	float speed;*/
	float dt;
	float2 windDir;
	float windSpeed;
	float4 waveA;
	float4 waveB;
	float4 waveC;
	float4 waveD;
	float2 direction;
	matrix projection;
	matrix lightView;
	matrix lightProj;
};


static const float PI = 3.14159f;

// Struct representing a single vertex worth of data
struct VertexShaderInput
{

	float3 position		: POSITION;     // XYZ position
	//float4 color		: COLOR;        // RGBA color
	float3 normal		: NORMAL;		//Normal of the vertex
	float3 tangent		: TANGENT;      //tangent of the vertex
	float2 uv			: TEXCOORD;		//Texture coordinates
};

// Struct representing the data we're sending down the pipeline
struct VertexToPixel
{

	float4 position		: SV_POSITION;	// XYZW position (System Value Position)
	float4 lightPos		: TEXCOORD1;
	float3 normal		: NORMAL;		//normal of the vertex
	float3 worldPosition: POSITION; //position of vertex in world space
	float3 tangent		: TANGENT;	//tangent of the vertex
	float2 uv			: TEXCOORD;
	float2 motion		: TEXCOORD2;
	float2 heightUV		: TEXCOORD3;
	noperspective float2 screenUV		: VPOS;
};

//function to calculate gerstner waves given a wave
float3 GerstnerWave(float4 wave, float3 pos, inout float3 tangent, inout float3 binormal, float dt)
{
	float steepness = wave.z;
	float wavelength = wave.w;

	float k = 2 * PI / wavelength;
	//modifying the y value and creating gerstner waves
	float c = sqrt(9.8f / k); //phase speed
	float2 d = normalize(wave.xy);
	float a = steepness / k;
	float f = k * (dot(d, pos.xz) - c / 2 * dt);

	tangent += float3(
		-d.x * d.x * (steepness * sin(f)),
		d.x * (steepness * cos(f)),
		-d.x * d.y * (steepness * sin(f)
			)
		);

	binormal += float3(
		-d.x * d.y * (steepness * sin(f)),
		d.y * (steepness * cos(f)),
		-d.y * d.y * (steepness * sin(f))
		);

	return float3(
		d.x * (a * cos(f)),
		a * sin(f),
		d.y * (a * cos(f))
		);
}
Texture2D heightMap: register(t0);
Texture2D heightMapX: register(t1);
Texture2D heightMapZ: register(t2);

SamplerState sampleOptions: register(s0);
static const float2 size = { 2.0,0.0 };
static const float3 off = { -1.0,0.0,1.0 };
// --------------------------------------------------------
// The entry point (main method) for our vertex shader
// 
// - Input is exactly one vertex worth of data (defined by a struct)
// - Output is a single struct of data to pass down the pipeline
// - Named "main" because that's the default the shader compiler looks for
// --------------------------------------------------------
VertexToPixel main(VertexShaderInput input)
{
	// Set up output struct
	VertexToPixel output;

	// First we multiply them together to get a single matrix which represents
	// all of those transformations (world to view to projection space)
	matrix worldViewProj = mul(mul(world, view), projection);

	float2 waveMotion = windSpeed * windDir;

	output.motion = waveMotion;

	float2 heightUV = float2(input.position.x, input.position.z) * 0.03f;
	heightUV.x = heightUV.x * 0.5 + 0.5;
	heightUV.y = -heightUV.y * 0.5 + 0.5;

	output.heightUV = input.uv;

	float texelSize = 1.0 / 256.0;

	float height = heightMap.SampleLevel(sampleOptions, input.uv, 0).r;
	float heightX = heightMapX.SampleLevel(sampleOptions, input.uv, 0).r * 1.8;
	float heightZ = heightMapZ.SampleLevel(sampleOptions, input.uv, 0).r * 1.8;

	float2 offxy = { off.x / 256.0 , off.y / 256.0 };
	float2 offzy = { off.z / 256.0 , off.y / 256.0 };
	float2 offyx = { off.y / 256.0 , off.x / 256.0 };
	float2 offyz = { off.y / 256.0 , off.z / 256.0 };

	//float hL = heightMap.SampleLevel(sampleOptions, float2(input.uv.x - texelSize, input.uv.y),0).r;
	//float hR = heightMap.SampleLevel(sampleOptions, float2(input.uv.x + texelSize, input.uv.y),0).r;
	//float hD = heightMap.SampleLevel(sampleOptions, float2(input.uv.x, input.uv.y - texelSize),0).r;
	//float hU = heightMap.SampleLevel(sampleOptions, float2(input.uv.x ,input.uv.y + texelSize),0).r;
	float s11 = height;
	float s01 = heightMap.SampleLevel(sampleOptions, input.uv + offxy, 0).r;
	float s21 = heightMap.SampleLevel(sampleOptions, input.uv + offzy, 0).r;
	float s10 = heightMap.SampleLevel(sampleOptions, input.uv + offyx, 0).r;
	float s12 = heightMap.SampleLevel(sampleOptions, input.uv + offyz, 0).r;
	float3 va = normalize(float3(2.0, 0, s21 - s01));
	float3 vb = normalize(float3(0.0, 2.0, s12 - s10));
	float3 normal = normalize(cross(vb, va));

	//float3 normal = float3(hR-hL, 2, hU-hD);
	//input.normal = normalize(normal);

	float3 pos = input.position;
	pos.y = height;
	pos.z -= heightZ;
	pos.x -= heightX;

	input.position = pos;
	//float3 tangent = float3(1.0f, 0, 0);
	//float3 binormal = float3(0.0f, 0, 1.0f);
	//pos += GerstnerWave(waveA,input.position,tangent,binormal,dt);
	//pos += GerstnerWave(waveB, input.position, tangent, binormal, dt);
	//pos += GerstnerWave(waveC, input.position, tangent, binormal, dt);
	//pos += GerstnerWave(waveD, input.position, tangent, binormal, dt);
	//input.position = pos;
	//input.tangernt = normalize(tangent);
	//input.normal = normalize(cross(binormal,tangent));

	// The result is essentially the position (XY) of the vertex on our 2D 
	// screen and the distance (Z) from the camera (the "depth" of the pixel)
	output.position = mul(float4(input.position, 1.0f), worldViewProj);

	//applying the normal by removing the translation from it
	output.normal = mul(input.normal, (float3x3)world);

	//sending the world position of the vertex to the fragment shader
	output.worldPosition = mul(float4(input.position, 1.0f), world).xyz;

	//sending the world coordinates of the tangent to the pixel shader
	output.tangent = mul(input.tangent, (float3x3)world);

	//sending the UV coordinates
	//output.uv = input.uv;
	output.uv = input.uv;

	matrix lightWorldViewProj = mul(mul(world, lightView), lightProj);

	output.screenUV = output.position.xy / output.position.w;
	output.screenUV.x = output.screenUV.x * 0.5 + 0.5;
	output.screenUV.y = -output.screenUV.y * 0.5 + 0.5;

	//sending the the shadow position
	output.lightPos = mul(float4(input.position, 1.0f), lightWorldViewProj);

	// Whatever we return will make its way through the pipeline to the pixel shader
	return output;
}