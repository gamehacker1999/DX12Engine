cbuffer externData: register(b0)
{
	int N;
	float normalStrength;
}

RWTexture2D<float4> normalMap: register(u0);
Texture2D<float4 >heightMap: register(t1);
SamplerState sampleOptions: register(s0);

[numthreads(16, 16, 1)]
void main(uint3 id : SV_DispatchThreadID)
{
	uint2 x = uint2(id.xy);
	float2 texCoord = x / float(N - 1);


	float texelSize = 1.0 / (float)N;

	/**/float z0 = heightMap.SampleLevel(sampleOptions, texCoord + float2(-texelSize, -texelSize), 0).r;
	float z1 = heightMap.SampleLevel(sampleOptions, texCoord + float2(0, -texelSize), 0).r;
	float z2 = heightMap.SampleLevel(sampleOptions, texCoord + float2(texelSize, -texelSize), 0).r;
	float z3 = heightMap.SampleLevel(sampleOptions, texCoord + float2(-texelSize, 0), 0).r;
	float z4 = heightMap.SampleLevel(sampleOptions, texCoord + float2(texelSize, 0), 0).r;
	float z5 = heightMap.SampleLevel(sampleOptions, texCoord + float2(-texelSize, texelSize), 0).r;
	float z6 = heightMap.SampleLevel(sampleOptions, texCoord + float2(0, texelSize), 0).r;
	float z7 = heightMap.SampleLevel(sampleOptions, texCoord + float2(texelSize, texelSize), 0).r;

	float3 normal;

	//sobel filter
	normal.z = 1 / normalStrength;
	normal.x = (z2 + 2.0 * z4 + z7) - (z0 + 2.0 * z3 + z5);
	normal.y = (z5 + 2.0 * z6 + z7) - (z0 + 2.0 * z1 + z2);

	normalMap[x] = float4((normalize(normal)), 1.0);

	//float fxy = heightMap[x].r;
	//float left = heightMap[uint2(x.x - 1, x.y)].r;
	//float right = heightMap[uint2(x.x + 1, x.y)].r;
	//float up = heightMap[uint2(x.x, x.y - 1)].r;
	//float down = heightMap[uint2(x.x, x.y + 1)].r;
	//
	//float3 normal = normalize(float3(2 * (right - left),2 * (down - up),-4	));
	//normalMap[x] = float4(normal, 1.0f);

}