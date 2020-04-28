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

struct VertexToPixel
{
	float4 position		: SV_POSITION;
	float3 worldPos		: TEXCOORD;
};

//cbuffer for matrix position
cbuffer SkyboxData: register(b0)
{
	matrix world;
	matrix view;
	matrix projection;
	float3 cameraPos;
}

//entry point of the skybox vertex shader
VertexToPixel main(VertexShaderInput input)
{
	VertexToPixel output; //this specifies the data that gets passed along the render pipeline

	matrix viewWithoutTranslate = view;
	viewWithoutTranslate._41 = 0;
	viewWithoutTranslate._42 = 0;
	viewWithoutTranslate._43 = 0;

	//view projection matrix
	matrix viewProj = mul(viewWithoutTranslate, projection);

	//calculating the vertex position
	float4 posWorld = float4(input.position, 1.0);

	//getting the perspective devide to be equal to one
	output.position = mul(posWorld, viewProj).xyww;

	//sending the world position to pixelshader
	output.worldPos = input.position;

	return output;
}