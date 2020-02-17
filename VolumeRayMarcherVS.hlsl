// Struct representing a single vertex worth of data
struct VertexShaderInput
{

	//  v    v                v
	float3 position		: POSITION;     // XYZ position
	//float4 color		: COLOR;        // RGBA color
	float3 normal		: NORMAL;		//Normal of the vertex
	float3 tangent		: TANGENT;      //tangent of the vertex
	float2 uv			: TEXCOORD;		//Texture coordinates
};

struct Ray
{
	float3 origin;
	float3 direction;
};

struct VertexToPixel
{
	float4 position		: SV_POSITION;
	float3 noisePos		: TEXCOORD;
	float3 worldPos		: TEXCOORD1;
};

//cbuffer for matrix position
cbuffer VolumeData: register(b0)
{
	matrix world;
	matrix inverseModel;
	matrix view;
	matrix viewInv;
	matrix projection;
	float3 cameraPos;
	float focalLength;
	float time;
}

//entry point of the skybox vertex shader
VertexToPixel main(VertexShaderInput input)
{
	VertexToPixel output; //this specifies the data that gets passed along the render pipeline

	//view projection matrix
	matrix worldviewProj = mul(world,mul(view, projection));

	float3 worldPos = mul(float4(input.position,1.0), world).xyz;

	//calculating the vertex position
	float4 posWorld = float4(input.position, 1.0);

	output.worldPos = mul(posWorld, world).xyz;

	//getting the perspective devide to be equal to one
	output.position = mul(posWorld, worldviewProj);

	float ticks = fmod(time, 10.f);

	//sending the world position to pixelshader
	output.noisePos = input.position;;

	return output;
}