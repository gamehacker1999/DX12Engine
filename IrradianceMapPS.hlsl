//struct to represent the vertex shader input
struct VertexToPixel
{
	float4 position		: SV_POSITION;
	float2 uv			: TEXCOORD;
};

cbuffer FaceIndex: register(b1)
{
	int faceIndex;
};

TextureCube skybox : register(t0);
SamplerState basicSampler: register(s0);

float4 main(VertexToPixel input) :SV_TARGET
{

	float2 o = input.uv * 2 - 1;

	// Tangent basis
	float3 xDir, yDir, zDir;

	// Figure out the z ("normal" of this pixel)
	switch (faceIndex)
	{
	default:
	case 0: zDir = float3(+1, -o.y, -o.x); break;
	case 1: zDir = float3(-1, -o.y, +o.x); break;
	case 2: zDir = float3(+o.x, +1, +o.y); break;
	case 3: zDir = float3(+o.x, -1, -o.y); break;
	case 4: zDir = float3(+o.x, -o.y, +1); break;
	case 5: zDir = float3(-o.x, -o.y, -1); break;
	}

	//calculating the irradience map
	float3 irradiance = float3(0.0f,0.0f,0.0f);

	float3 normal = normalize(zDir);

	float3 up = float3(0.0f, 1.0f, 0.0f);

	float3 right = normalize(cross(up,normal));
	up = normalize(cross(normal, right));

	float PI = 3.141592653f;

	float sampleDelta = 0.025f;
	float numOfSamples = 0.0f;
	//solving the monte carlo integral of the irradience equation
	//[loop]
	for (float phi = 0.0f; phi < 2.0f * PI; phi += sampleDelta)
	{
		//[loop]
		for (float theta = 0.0f; theta < PI * 0.5f; theta += sampleDelta)
		{
			//convert spherical coordinates to cartesian space
			float3 cartesian = float3(sin(theta) * cos(phi), sin(theta) * sin(phi), cos(theta));

			//connverting from tangent space to world space
			float3 sampleVector = (cartesian.x * right) + (cartesian.y * up) + (cartesian.z * normal);

			//adding all the texure values
			irradiance += skybox.Sample(basicSampler, sampleVector).rgb * sin(theta) * cos(theta);

			numOfSamples++;
		}
	}

	//dividing by the total number of samples
	irradiance = PI * irradiance * (1 / numOfSamples);

	//returning the sample value
	return float4(irradiance,  1.0f);

}