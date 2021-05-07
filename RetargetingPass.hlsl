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

[numthreads(8, 8, 1)]
void main(uint3 groupID : SV_GroupID, // 3D index of the thread group in the dispatch.
          uint3 groupThreadID : SV_GroupThreadID, // 3D index of local thread ID in a thread group.
          uint3 dispatchThreadID : SV_DispatchThreadID, // 3D index of global thread ID in the dispatch.
          uint groupIndex : SV_GroupIndex)
{
    
    int texWidth;
    int texHeight;
    
    
    retargetTex.GetDimensions(texWidth, texHeight);
    
    float2 offset = GenerateR2Sequence(frameNum);
    offset.x *= texWidth-1;
    offset.y *= texHeight-1;
    
    
    int2 samplePos = dispatchThreadID.xy+offset;
    
    samplePos %= uint2(texWidth, texHeight);
    
    float2 pixelOffsets = retargetTex[samplePos].gb ;
    
    pixelOffsets.x *= texWidth;
    pixelOffsets.y *= texHeight;
    
    int2 finalLoc = dispatchThreadID.xy + int2(pixelOffsets.x, pixelOffsets.y);
    
    finalLoc %= uint2(1920, 1080);
    
    uint inIndex = (dispatchThreadID.y) * 1920 + (dispatchThreadID.x);
    uint num = newSequences.Load(inIndex);
    uint outIndex = uint(finalLoc.y) * 1920 + uint(finalLoc.x);
    outSequences[outIndex] = num;

}