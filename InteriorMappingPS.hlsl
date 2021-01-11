struct VertexToPixel
{
    float4 position : SV_POSITION;
    float4 color : COLOR;
    float3 normal : NORMAL;
    float3 tangent : TANGENT;
    float3 worldPosition : POSITION;
    float2 uv : TEXCOORD;

};

cbuffer externalData : register(b0)
{
    float3 cameraPosition;
    int Offices;
    int NumCubeMaps;
    int RandSeed;
}

// Pseudo - random number generator from :
// https://gist.github.com/keijiro/ee7bc388272548396870
float rand(float2 uv)
{
    return frac(sin(dot(uv, float2(12.9898, 78.233))) * 43758.5453);
}

// Calculates a random index within the specified array length
int RandomArrayIndex(float2 seed, int arrayLength)
{
    return (int) (rand(trunc(seed)) * arrayLength);
}

// Rotates a 3D direction randomly either 0, 90, 180 or 270 degrees
float3 RandomCubeRotation(float3 direction, float2 seed)
{
    
    
	// Rotate "randomly" either 0, 90, 180 or 270 degrees
    const float halfpi = 1.57079632679f;
    float rotate = trunc(rand(trunc(seed)) * 4) * halfpi;
    
    if (seed.x < 1)
    {
        rotate = 0;
    }
    
    if (seed.x > 1*5 && seed.x<2*5)
    {
        rotate = 270 * 2.0* halfpi/180.0;
    }
    
    if (seed.x > 2*5 && seed.x < 3*5)
    {
        rotate = 180 * 2.0 * halfpi / 180.0;
    }
    
    if (seed.x > 3*5 && seed.x < 4*5)
    {
        rotate = 90 * 2.0 * halfpi / 180.0;
    }

	// Get sin and cos in one function call
    float s, c;
    sincos(rotate, s, c);

	// Build a Y-rotation matrix and rotate
    float3x3 rotMat = float3x3(
		c, 0, s,
		0, 1, 0,
		-s, 0, c);
    return mul(direction, rotMat);
}


// Texture-related resources
TextureCubeArray interiorCube : register(t0);
Texture2D exteriorTexture : register(t1);
Texture2D capTexture : register(t2);
Texture2D sdfTexture : register(t3);
SamplerState samplerOptions : register(s0);
SamplerState linearBorder : register(s1);

float4 main(VertexToPixel input) : SV_TARGET
{
	
	// Renormalize your interpolated normals / tangents
    input.normal = normalize(input.normal);
    input.tangent = normalize(input.tangent);
	
	// Create a 3x3 rotation matrix that represents how
	// the texture lays across the triangle at this vertex
    float3 N = input.normal;
    float3 T = normalize(input.tangent - N * dot(input.tangent, N)); // Orthogonalize
    float3 B = cross(T, N);
    float3x3 TBN = float3x3(T, -B, N); // Negate the bi-tangent!

	// Calculate the view vector
    float3 view = normalize(input.worldPosition - cameraPosition);

	// Reflection vector for window/sky reflection (handled here in world space)
    float3 refl = reflect(view, input.normal);
    
    //scaling the uv to show the multiple buildings
    float2 uvScaled = input.uv * 3;
    //getting the faction part of the uv
    float2 uvFractional = frac(uvScaled);

    //Raytracing to find the intersection of the view ray and the body
    
    float3 uvPosition = float3(uvFractional * 2.0 - 1.0, 1.0f);
    view = mul(view, transpose(TBN));
    
    //method2
    float3 roomSize = float3(1, 1, 1);
    float3 roomOrigin = frac(float3(uvScaled, 0)) / roomSize;
    float3 rayDirection = normalize(view)/ roomSize;
    
    //define the volume of the room 
    float3 boxMin = floor(float3(uvFractional, -1));
    float3 boxMax = boxMin + 1.0;
    float3 boxMid = boxMin + 0.5;
    
    float3 planes = lerp(boxMin, boxMax, step(0, rayDirection));
    float3 minPlane = ((planes - roomOrigin) / rayDirection);
    
    float dist = min(min(minPlane.x, minPlane.y), minPlane.z);
    
    //ray vector
    float3 sampleVec = (roomOrigin + rayDirection * dist) - boxMid;
    
    int cubeIndex = RandomArrayIndex(uvScaled + RandSeed, NumCubeMaps);
    
    float3 roomPos = roomOrigin + rayDirection * dist;
    
    
    //Calculating shadow ray
    float2 shadowOrigin = roomPos.xy;
    
    float3 lightDir = float3(0.7, -0.5, 0);
    
    lightDir = normalize(lightDir);
    //converting light to tangent space
    float3 tLightDir = mul(lightDir, transpose(TBN));
    
    //calculate light dir
    float2 shadowDir = (-tLightDir.xy / tLightDir.z);
    
    //calculate the shadow coordinate
    float2 shadowCoordinate = shadowOrigin + shadowDir * roomPos.z;
    
    shadowCoordinate = saturate(shadowCoordinate) * (1.0/3.0);
    
    shadowCoordinate.y = 1.f - shadowCoordinate.y;
    
    float shadowDist = sdfTexture.Sample(linearBorder, shadowCoordinate).a;
    
    float shadowThreshold = saturate(0.5 + 0.3 * (-roomPos.z * (1.0 / 3.0)));
    float shadow = smoothstep(0.5, shadowThreshold, shadowDist);
    shadow *= 0.8f;

    shadow = 1.f - shadow;
    
    shadow = lerp(shadow, 1, step(0, tLightDir.z));
    
    float ndotl = saturate(dot(N, -lightDir));
    
    
     // Sample the interior cube map and interpolate between that and the reflection 
    float4 interiorColor = interiorCube.Sample(samplerOptions, float4(sampleVec, cubeIndex));
    float4 windowColor = interiorColor;
    
    //Sample external wall
    float4 externalColor = exteriorTexture.Sample(samplerOptions, input.uv);
    float4 capColor = capTexture.Sample(samplerOptions, input.uv);
    
    float3 finalColor = lerp(windowColor.xyz * shadow, externalColor.xyz/3.15159, externalColor.a);
    
    N.y = round(N.y);
    N.y = abs(N.y);
    
    if (N.y == 1)
    {
        finalColor = capColor.xyz / 3.15159;
    }
    
    return float4(finalColor, 1.0f);
    
}

