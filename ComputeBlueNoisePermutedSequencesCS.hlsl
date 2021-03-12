
Texture2D blueNoise : register(t0);
Texture2D prevFrame : register(t1);
Texture2D retargetTex : register(t2);
RWStructuredBuffer<uint> newSequences : register(u0);
SamplerState pointSampler : register(s0);

#define X_OFFSET 0.2
#define Y_OFFSET 0.2

cbuffer ExternData : register(b0, space1)
{
    uint frameNum;
}

// Generates a seed for a random number generator from 2 inputs plus a backoff
uint InitSeed(uint val0, uint val1, uint backoff = 16)
{
    uint v0 = val0, v1 = val1, s0 = 0;

    for (uint n = 0; n < backoff; n++)
    {
        s0 += 0x9e3779b9;
        v0 += ((v1 << 4) + 0xa341316c) ^ (v1 + s0) ^ ((v1 >> 5) + 0xc8013ea4);
        v1 += ((v0 << 4) + 0xad90777d) ^ (v0 + s0) ^ ((v0 >> 5) + 0x7e95761e);
    }
    return v0;
}
#define BLOCK 4
#define F_BLOCK 4.0f

// A function to implement bubble sort  
void BubbleSort(inout float3 arr1[BLOCK * BLOCK])
{
    
    float3 arr[BLOCK*BLOCK] = arr1;
    
    int i, j;
    for (i = 0; i < BLOCK * BLOCK - 1; i++)
    {
    
        for (j = 0; j < BLOCK * BLOCK - i - 1; j++)
        {
        
            if (arr[j].r > arr[j + 1].r)
            {
                float3 temp = arr[j];
                arr[j] = arr[j + 1];
                arr[j + 1] = temp;
            }
        }
    }
    
    arr1 = arr;
}

//http://extremelearning.com.au/unreasonable-effectiveness-of-quasirandom-sequences/
float2 GenerateR2Sequence(uint n)
{
    float g = 1.32471795724474602596;

    float a1 = 1.0 / g;

    float a2 = 1.0 / (g * g);

    float x = (0.5 + a1 * n) % 1;

    float y = (0.5 + a2 * n) % 1;

    return float2(x, y);
}

//https://gamedev.stackexchange.com/questions/135947/how-to-sort-tiled-decal-list
void BitonicSort(inout float3 arr1[BLOCK * BLOCK], uint groupIndex)
{
    uint numArray = BLOCK*BLOCK;

    // Round the number of items up to the nearest power of two
    uint numArrayPowerOfTwo = 1;
    while (numArrayPowerOfTwo < numArray)
        numArrayPowerOfTwo <<= 1;

    GroupMemoryBarrierWithGroupSync();

    for (uint nMergeSize = 2; nMergeSize <= numArrayPowerOfTwo; nMergeSize = nMergeSize * 2)
    {
        for (uint nMergeSubSize = nMergeSize >> 1; nMergeSubSize > 0; nMergeSubSize = nMergeSubSize >> 1)
        {
            uint tmp_index = groupIndex; // It is the SV_GroupIndex, the flattened index of the thread within the threadgroup
            uint index_low = tmp_index & (nMergeSubSize - 1);
            uint index_high = 2 * (tmp_index - index_low);
            uint index = index_high + index_low;

            uint nSwapElem = nMergeSubSize == nMergeSize >> 1 ? index_high + (2 * nMergeSubSize - 1) - index_low : index_high + nMergeSubSize + index_low;
            if (nSwapElem < numArray && index < numArray)
            {
                if (arr1[index].r > arr1[nSwapElem].r)
                {
                    float3 uTemp = arr1[index];
                    arr1[index] = arr1[nSwapElem];
                    arr1[nSwapElem] = uTemp;
                }
            }
            GroupMemoryBarrierWithGroupSync();
        }
    }
}
  
float CalcIntensity(float3 color)
{
    return (color.r*0.3f + color.g*0.59f + color.b*0.11f);
}

// this noise, scrolling constant are from Jorge Jimenez
float InterleavedGradientNoise(float2 pixel)
{
    return frac(52.9829189f * frac(0.06711056f * float(pixel.x) + 0.00583715f * float(pixel.y)));
}



groupshared uint newRandSeeds[BLOCK * BLOCK];
groupshared float3 colors[BLOCK * BLOCK];
groupshared float3 blueNoiseColors[BLOCK * BLOCK];
groupshared uint randSeeds[BLOCK * BLOCK];

[numthreads(BLOCK, BLOCK, 1)]
void main(uint3 groupID : SV_GroupID, // 3D index of the thread group in the dispatch.
uint3 groupThreadID : SV_GroupThreadID, // 3D index of local thread ID in a thread group.
uint3 dispatchThreadID : SV_DispatchThreadID, // 3D index of global thread ID in the dispatch.
uint groupIndex : SV_GroupIndex)
{
    
    float2 offset = GenerateR2Sequence(frameNum);
    
    uint initialSeed = InitSeed(dispatchThreadID.x, dispatchThreadID.y);
    randSeeds[groupIndex] = initialSeed;
    
    if(frameNum == 0)
    {
        newSequences[(dispatchThreadID.y) * 1920 + (dispatchThreadID.x)] = initialSeed;
        return;

    }

    float2 samplePoint = float2(dispatchThreadID.xy) / float2((1920.f), (1080.f));
    
    colors[groupIndex] = float3(CalcIntensity(prevFrame.SampleLevel(pointSampler, samplePoint, 0).rgb), groupThreadID.x, groupThreadID.y);
    
    samplePoint += (float2(offset.x, offset.y) * 2.f - 1.f) / float2((512.f), (512.f));


    blueNoiseColors[groupIndex] = float3((blueNoise.SampleLevel(pointSampler, samplePoint, 0)).r, groupThreadID.x, groupThreadID.y);
    
    
    GroupMemoryBarrierWithGroupSync();
    
    //if (groupIndex == 0)
    //{
    //   BitonicSort(colors, groupIndex);
    //   BitonicSort(blueNoiseColors, groupIndex);
   // }
    
    if (groupIndex == 0)
    {
        BubbleSort(colors);
        BubbleSort(blueNoiseColors);
    }
    
    GroupMemoryBarrierWithGroupSync();

    
    newRandSeeds[blueNoiseColors[groupIndex].g * F_BLOCK + blueNoiseColors[groupIndex].b] = randSeeds[colors[groupIndex].g * F_BLOCK + colors[groupIndex].b];
    GroupMemoryBarrierWithGroupSync();

    newSequences[(dispatchThreadID.y) * 1920 + (dispatchThreadID.x)] = newRandSeeds[groupIndex];

    


}