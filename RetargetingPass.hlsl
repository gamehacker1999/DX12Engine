RWStructuredBuffer<uint> outSequences : register(u0);
RWStructuredBuffer<uint> newSequences : register(u1);
Texture2D retargetTex : register(t0);
SamplerState pointSampler : register(s0);

cbuffer ExternData : register(b0)
{
    uint frameNum;
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

[numthreads(32, 32, 1)]
void main(uint3 groupID : SV_GroupID, // 3D index of the thread group in the dispatch.
          uint3 groupThreadID : SV_GroupThreadID, // 3D index of local thread ID in a thread group.
            uint3 dispatchThreadID : SV_DispatchThreadID, // 3D index of global thread ID in the dispatch.
            uint groupIndex : SV_GroupIndex)
{
    float2 offset = GenerateR2Sequence(frameNum);
    
    float2 retargetPoint = float2(dispatchThreadID.xy) / float2((1920.f), (1080.f));
    retargetPoint += (float2(offset.x, offset.y) * 2.f - 1.f) / float2((512.f), (512.f));
    
    float2 value = float2((retargetTex.SampleLevel(pointSampler, retargetPoint, 0)).rg);
    
    value.x += (offset.x * 2.f -1.f) / 512.f;
    value.y += (offset.y * 2.f - 1.f) / 512.f;
    
    value.x *= 1920;
    value.y *= 1080;
        
    uint num = newSequences.Load((dispatchThreadID.y) * 1920 + (dispatchThreadID.x));
    uint outIndex = uint(value.y) * 1920 + uint(value.x);
    outSequences[outIndex] = num;

}