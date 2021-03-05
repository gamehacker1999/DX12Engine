
Texture2D blueNoise : register(t0);
Texture2D prevFrame : register(t1);
RWStructuredBuffer<uint> newSequences : register(u0);
SamplerState pointSampler : register(s0);

#define X_OFFSET 2
#define Y_OFFSET 2

cbuffer ExternData : register(b0, space1)
{
    uint frameNum;
}

// Generates a seed for a random number generator from 2 inputs plus a backoff
uint InitRand(uint val0, uint val1, uint backoff = 16)
{
    uint v0 = val0, v1 = val1, s0 = 0;

	[unroll]
    for (uint n = 0; n < backoff; n++)
    {
        s0 += 0x9e3779b9;
        v0 += ((v1 << 4) + 0xa341316c) ^ (v1 + s0) ^ ((v1 >> 5) + 0xc8013ea4);
        v1 += ((v0 << 4) + 0xad90777d) ^ (v0 + s0) ^ ((v0 >> 5) + 0x7e95761e);
    }
    return v0;
}
#define BLOCK 4

// A function to implement bubble sort  
void BubbleSort(inout float2 arr1[BLOCK * BLOCK])
{
    
    float2 arr[BLOCK*BLOCK] = arr1;
    
    int i, j;
    for (i = 0; i < BLOCK * BLOCK - 1; i++)
    {
    
        for (j = 0; j < BLOCK * BLOCK - i - 1; j++)
        {
        
            if (arr[j].r > arr[j + 1].r)
            {
                float2 temp = arr[j];
                arr[j] = arr[j + 1];
                arr[j + 1] = temp;
            }
        }
    }
    
    arr1 = arr;
}
  
float CalcIntensity(float3 color)
{
    return 0.299 * color.r + 0.587*color.g + 0.114*color.b;
}


groupshared uint newRandSeeds[BLOCK * BLOCK];
groupshared float2 colors[BLOCK * BLOCK];
groupshared float2 blueNoiseColors[BLOCK * BLOCK];
groupshared uint randSeeds[BLOCK * BLOCK];

[numthreads(BLOCK, BLOCK, 1)]
void main(uint3 groupID : SV_GroupID, // 3D index of the thread group in the dispatch.
uint3 groupThreadID : SV_GroupThreadID, // 3D index of local thread ID in a thread group.
uint3 dispatchThreadID : SV_DispatchThreadID, // 3D index of global thread ID in the dispatch.
uint groupIndex : SV_GroupIndex)
{
    uint initialRand = InitRand(dispatchThreadID.x, dispatchThreadID.y);
    randSeeds[groupIndex] = initialRand;
    
    //float2 samplePoint = float2(dispatchThreadID.xy) / float2((1920.f), (1080.f));
    
    colors[groupIndex] = float2(CalcIntensity(prevFrame.Load(dispatchThreadID).rgb), groupIndex);
    blueNoiseColors[groupIndex] = float2(blueNoise.Load(dispatchThreadID).r, groupIndex);
    
    GroupMemoryBarrierWithGroupSync();
    
    
    if(groupIndex == 0)
    {
        BubbleSort(colors);
        BubbleSort(blueNoiseColors);
    }
    
    GroupMemoryBarrierWithGroupSync();
    
    newRandSeeds[(blueNoiseColors[groupIndex].g)] = randSeeds[(colors[groupIndex].g)];
    
    GroupMemoryBarrierWithGroupSync();
    
    uint num = newRandSeeds[groupIndex];
    
    newSequences[(dispatchThreadID.x) * 1080 + (dispatchThreadID.y)] = num;


}