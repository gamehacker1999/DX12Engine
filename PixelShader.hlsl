struct DirectionalLight
{
	float4 ambientColor;
	float4 diffuse;
	float4 specularity;
	float3 direction;
};

cbuffer LightData: register(b1)
{
	DirectionalLight light1;
	float3 cameraPosition;
};

struct Index
{
	uint index;
};

ConstantBuffer<Index> entityIndex: register(b0);

struct VertexToPixel
{
	float4 position: SV_POSITION;
	float4 color: COLOR;
	float3 normal:  NORMAL;
	float3 tangent: TANGENT;
	float3 worldPosition: POSITION;
	float2 uv: TEXCOORD;

};

//function that accepts light and normal and then calculates the final color
float4 CalculateLight(DirectionalLight light, float3 normal, VertexToPixel input)
{
	//standard N dot L calculation for the light
	float3 L = -light.direction;
	L = normalize(L); //normalizing the negated direction
	float3 N = normal;
	N = normalize(N); //normalizing the normal
	float3 R = reflect(-L, N); //reflect R over N
	float3 V = normalize(cameraPosition - input.worldPosition); //view vector
	float4 NdotV = saturate(dot(N, V));
	float4 rimColor = float4(0.0f, 0.0f, 1.0f, 1.0f);

	//calculate the cosine of the angle to calculate specularity
	//I am calculating the light based on the phong reflection model
	float cosine = dot(R, V);
	cosine = saturate(cosine);
	float shininess = 8.f;
	float specularAmount = pow(cosine, shininess); //increase the cosine curve fall off

	float NdotL = dot(N, L);
	NdotL = saturate(NdotL); //this is the light amount, we need to clamp it to 0 and 1.0

	//float4 diffuse = celShading.Sample(basicSampler, NdotL);
	//return diffuse;

	//adding diffuse, ambient, and specular color
	float4 finalLight = light.diffuse * NdotL;
	finalLight += specularAmount;

	return finalLight;
}

Texture2D material[]: register(t0);
SamplerState basicSampler: register(s0);

float4 main(VertexToPixel input) : SV_TARGET
{

	uint index = entityIndex.index;

	float4 texColor = material[index].Sample(basicSampler,input.uv);

	float3 normal = material[index + 1].Sample(basicSampler, input.uv).xyz;

	float3 unpackedNormal = normal * 2.0 - 1.0f;

	float3 N = normalize(input.normal);
	float3 T = input.tangent - dot(input.tangent, N) * N;
	T = normalize(T);
	float3 B = normalize(cross(T, N));

	float3x3 TBN = float3x3(T, B, N);

	float3 finalNormal = mul(unpackedNormal, TBN);

	return float4(CalculateLight(light1,finalNormal,input))*texColor;
}