#include "Common.hlsl"
//#include "Lighting.hlsli"
//
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
//
////struct to desctibe the directional light
//struct DirectionalLight
//{
//	float4 ambientColor;
//	float4 diffuse;
//	float4 specularity;
//	float3 direction;
//};
//
////constant buffer to hold light data
//cbuffer LightData: register(b0)
//{
//	//DirectionalLight light;
//	//DirectionalLight light2;
//	float scrollX;
//	float scrollY;
//	float3 cameraPosition;
//	matrix view;
//	Light dirLight;
//	//Light lights[MAX_LIGHTS];
//	//int lightCount;
//};
//
////function that accepts light and normal and then calculates the final color
//float4 CalculateLight(float3 normal, VertexToPixel input)
//{
//	//standard N dot L calculation for the light
//	float3 L = -dirLight.direction;
//	L = normalize(L); //normalizing the negated direction
//	float3 N = normal;
//	N = normalize(N); //normalizing the normal
//	float3 R = reflect(-L, N); //reflect R over N
//	float3 V = normalize(cameraPosition - input.worldPosition); //view vector
//	float4 NdotV = saturate(dot(N, V));
//	float4 rimColor = float4(0.0f, 0.0f, 1.0f, 1.0f);
//
//	//calculate the cosine of the angle to calculate specularity
//	//I am calculating the light based on the phong reflection model
//	float cosine = dot(R, V);
//	cosine = saturate(cosine);
//	float shininess = 64.f;
//	float specularAmount = pow(cosine, shininess); //increase the cosine curve fall off
//
//	float NdotL = dot(N, L);
//	NdotL = saturate(NdotL); //this is the light amount, we need to clamp it to 0 and 1.0
//	//return diffuse;
//
//	//adding diffuse, ambient, and specular color
//	float4 finalLight = float4(dirLight.diffuse, 1.0f) * NdotL;
//	//finalLight += specularAmount;
//	finalLight += float4(0.4, 0.4, 0.4, 1.0f);
//
//	return finalLight;
//}
//
//Texture2D waterTexture: register(t0);
//Texture2D normalTexture1: register(t1);
//Texture2D normalTexture2: register(t2);
//Texture2D normalTexture3: register(t3);
//Texture2D reflectionTexture: register(t4);
//TextureCube cubeMap: register(t5);
//Texture2D foam: register(t6);
//Texture2D foldingMap: register(t7);
//SamplerState sampleOptions: register(s0);
//SamplerState waterSampleOptions: register(s1);
//
float4 main(VertexToPixel input) : SV_TARGET
{
	//
	//	float2 reflectionTexCoord = float2(input.screenUV.x, -input.screenUV.y);
	//	float4 reflectionColor = reflectionTexture.Sample(sampleOptions, reflectionTexCoord);
	//
	//	float2 scrollUV1 = float2(input.uv.x, input.uv.y);
	//	float3 normal1 = normalTexture1.Sample(sampleOptions, scrollUV1).rgb;
	//	//unpacking the normal
	//	normal1 = (normal1 * 2.0f) - 1.0f;
	//
	//	//input.normal = normalize(cross(ddx_fine(input.worldPosition), ddy_fine(input.worldPosition)));
	//
	//	float2 scrollUV2 = float2(input.uv.x, input.uv.y);
	//	float3 normal2 = normalTexture2.Sample(sampleOptions, scrollUV2).rgb;
	//
	//	normal2 = (normal2 * 2.0f) - 1.0f;
	//
	//	float3 normal3 = saturate(normalTexture3.Sample(sampleOptions, input.uv)).rgb;
	//
	//	normal3 = (normal3 * 2.0f) - 1.0f;
	//
	//	float4 surfaceColor = waterTexture.Sample(sampleOptions, input.uv);
	//
	//	float4 foamColor = foam.Sample(sampleOptions, input.uv * 15);
	//	float4 foldingColor = foldingMap.Sample(sampleOptions, input.uv);
	//
	//	//surfaceColor = pow(surfaceColor, 2.2);
	//	//surfaceColor += float4(0.2, 0.2, 0.2,0);
	//	surfaceColor += saturate((foamColor * foldingColor));
	//
	//	float3 N = normalize(input.normal);
	//	float3 T = normalize(input.tangent) - (dot(normalize(input.tangent), N) * N);
	//	float3 B = normalize(cross(T, N));
	//	float3x3 TBN = float3x3(T, B, N);
	//
	//	normal1 = mul(normal1, TBN);
	//	normal2 = mul(normal2, TBN);
	//	normal3 = mul(normal3, TBN);
	//
	//	//averaging the two normals
	//	float3 finalNormal = normalize(normal1 + normal2);
	//	finalNormal = normalize(normal3);
	//	//surfaceColor = pow(surfaceColor, 2.2);
	//
	//	//calculating cubemap reflections
	//	float3 I = input.worldPosition - cameraPosition;
	//	I = normalize(I); //incident ray
	//	float3 reflected = reflect(I, finalNormal);
	//
	//	float2 reflectedUV = mul(float4(reflected, 0),view).xy * 0.1;
	//	reflectedUV.x *= -1;
	//
	//	reflectionColor = reflectionTexture.Sample(sampleOptions, reflectionTexCoord);
	//	float4 lightingColor = CalculateLight(finalNormal, input);
	//
	//	float4 totalColor = surfaceColor * lightingColor;
	//
	//	//totalColor = pow(totalColor, 1 / 2.2);
	//
	//	float3 V = normalize(cameraPosition - input.worldPosition); //view vector
	//	float4 NdotV = saturate(dot(N, V));
	//	//return totalColor;
	//	return lerp(reflectionColor,totalColor, 0.7);
		return float4(1,0,0,1);
}