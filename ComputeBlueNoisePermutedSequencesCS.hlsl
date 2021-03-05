
Texture2D blueNoise : register(t0);
Texture2D prevFrame : register(t1);
RWStructuredBuffer<float> newSequences : register(u0);
SamplerState pointSampler : register(s0);

cbuffer ExternData : register(b0)
{
    uint frameNum;
    float frameWidth;
    float frameHeight;
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

// A function to implement bubble sort  
void BubbleSortNoise(inout float2 arr[16])
{
    int i, j;
    for (i = 0; i < 16 - 1; i++)
    {
    
        for (j = 0; j < 16 - i - 1; j++)
        {
        
            if (arr[j].r > arr[j + 1].r)
            {
                float2 temp = arr[j];
                arr[j] = arr[j + 1];
                arr[j + 1] = temp;
            }
        }
    }
}
  
float CalcIntensity(float3 color)
{
    return 0.299 * color.r + 0.587*color.g + 0.114*color.b;
}

groupshared float newRandSeeds[16];

[numthreads(4, 4, 1)]
void main(uint3 groupID : SV_GroupID, // 3D index of the thread group in the dispatch.
uint3 groupThreadID : SV_GroupThreadID, // 3D index of local thread ID in a thread group.
uint3 dispatchThreadID : SV_DispatchThreadID, // 3D index of global thread ID in the dispatch.
uint groupIndex : SV_GroupIndex)
{
    if(frameNum == 0)
    {
        newSequences[dispatchThreadID.y*frameWidth+dispatchThreadID.x] = InitRand(dispatchThreadID.x, dispatchThreadID.y);
    }
    
    if (groupIndex==0)
    {
        float2 colors[16];
        float2 blueNoiseColors[16];
        float randSeeds[16] =
        {
            InitRand(dispatchThreadID.x, dispatchThreadID.y),
            InitRand(dispatchThreadID.x+1, dispatchThreadID.y),
            InitRand(dispatchThreadID.x+2, dispatchThreadID.y),
            InitRand(dispatchThreadID.x+3, dispatchThreadID.y),
            InitRand(dispatchThreadID.x, dispatchThreadID.y+1),
            InitRand(dispatchThreadID.x+1, dispatchThreadID.y+1),
            InitRand(dispatchThreadID.x+2, dispatchThreadID.y+1),
            InitRand(dispatchThreadID.x+3, dispatchThreadID.y+1),
            InitRand(dispatchThreadID.x, dispatchThreadID.y + 2),
            InitRand(dispatchThreadID.x+1, dispatchThreadID.y+2),
            InitRand(dispatchThreadID.x+2, dispatchThreadID.y+2),
            InitRand(dispatchThreadID.x+3, dispatchThreadID.y+2),
            InitRand(dispatchThreadID.x, dispatchThreadID.y + 3),
            InitRand(dispatchThreadID.x+1, dispatchThreadID.y+3),
            InitRand(dispatchThreadID.x+2, dispatchThreadID.y+3),
            InitRand(dispatchThreadID.x+3, dispatchThreadID.y+3),

        };
        
        float2 samplePoints[16] =
        {
            float2(dispatchThreadID.xy) / float2(frameWidth, frameHeight),
            float2(dispatchThreadID.x + 1, dispatchThreadID.y) / float2(frameWidth, frameHeight),
            float2(dispatchThreadID.x + 2, dispatchThreadID.y) / float2(frameWidth, frameHeight),
            float2(dispatchThreadID.x + 3, dispatchThreadID.y) / float2(frameWidth, frameHeight),
            float2(dispatchThreadID.x, dispatchThreadID.y + 1) / float2(frameWidth, frameHeight),
            float2(dispatchThreadID.x + 1, dispatchThreadID.y + 1) / float2(frameWidth, frameHeight),
            float2(dispatchThreadID.x + 2, dispatchThreadID.y + 1) / float2(frameWidth, frameHeight),
            float2(dispatchThreadID.x + 3, dispatchThreadID.y + 1) / float2(frameWidth, frameHeight),
            float2(dispatchThreadID.x, dispatchThreadID.y + 2) / float2(frameWidth, frameHeight),
            float2(dispatchThreadID.x + 1, dispatchThreadID.y + 2) / float2(frameWidth, frameHeight),
            float2(dispatchThreadID.x + 2, dispatchThreadID.y + 2) / float2(frameWidth, frameHeight),
            float2(dispatchThreadID.x + 3, dispatchThreadID.y + 2) / float2(frameWidth, frameHeight),
            float2(dispatchThreadID.x, dispatchThreadID.y + 3) / float2(frameWidth, frameHeight),
            float2(dispatchThreadID.x + 1, dispatchThreadID.y + 3) / float2(frameWidth, frameHeight),
            float2(dispatchThreadID.x + 2, dispatchThreadID.y + 3) / float2(frameWidth, frameHeight),
            float2(dispatchThreadID.x + 3, dispatchThreadID.y + 3) / float2(frameWidth, frameHeight)

        };
       
        
        for (int i = 0; i < 16; i++)
        {
            colors[i] = float2(CalcIntensity(prevFrame.SampleLevel(pointSampler, samplePoints[i], 0).rgb), i);
            blueNoiseColors[i] = float2(blueNoise.SampleLevel(pointSampler, samplePoints[i], 0).r, i);

        }
        
        BubbleSortNoise(colors);
        BubbleSortNoise(blueNoiseColors);
        
        for (int i = 0; i < 16; i++)
        {
            newRandSeeds[blueNoiseColors[i].g] = randSeeds[colors[i].g];
        }

    }
    
    GroupMemoryBarrierWithGroupSync();
    
    newSequences[(dispatchThreadID.x) + (dispatchThreadID.y)*4] = newRandSeeds[groupIndex];


}