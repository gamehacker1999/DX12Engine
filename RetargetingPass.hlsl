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

    float x = (a1 * n) % 1;

    float y = (a2 * n) % 1;

    return float2(x, y);
}
// Generates a seed for a random number generator from 2 inputs plus a backoff
uint InitSeed2(uint3 thread, uint width)
{
    uint rngSeed = (thread.x) + (thread.y) * (width);
    
    return rngSeed;
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

[numthreads(8, 8, 1)]
void main(uint3 groupID : SV_GroupID, // 3D index of the thread group in the dispatch.
          uint3 groupThreadID : SV_GroupThreadID, // 3D index of local thread ID in a thread group.
          uint3 dispatchThreadID : SV_DispatchThreadID, // 3D index of global thread ID in the dispatch.
          uint groupIndex : SV_GroupIndex)
{
        
    int texWidth;
    int texHeight;
      
    retargetTex.GetDimensions(texWidth, texHeight);
    
    if(frameNum == 0)
    {
        uint inIndex = (dispatchThreadID.y) * 1920 + (dispatchThreadID.x);
        uint num = newSequences.Load(inIndex);
        outSequences[inIndex] = num;
        
        return;
    }
    
    float2 offset = GenerateR2Sequence(frameNum);
    
    uint samplePosX = (dispatchThreadID.x + offset.x * (texWidth - 1)) % texWidth;
    uint samplePosY = (dispatchThreadID.y + offset.y * (texHeight - 1)) % texHeight;
    
    float2 pixelOffsets = retargetTex[uint2(samplePosX, samplePosY)].gb;
    
    pixelOffsets *= 255;
    
    if(pixelOffsets.x>128)
    {
        pixelOffsets.x = pixelOffsets.x - 256;
    }
    
    if (pixelOffsets.y > 128)
    {
        pixelOffsets.y = pixelOffsets.y - 256;
    }
    
    int2 finalLoc = dispatchThreadID.xy) + int2(pixelOffsets.x, pixelOffsets.y);
    
    finalLoc %= uint2(1920, 1080);
    
    uint inIndex = (dispatchThreadID.y) * 1920 + (dispatchThreadID.x);
    uint num = newSequences.Load(inIndex);
    uint outIndex = uint(finalLoc.y) * 1920 + uint(finalLoc.x);
    outSequences[outIndex] = num;

}