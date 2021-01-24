
Texture2D directLighting : register(t0);
Texture2D indirectDiffuse : register(t1);
Texture2D indirectSpecular : register(t2);
SamplerState basicSampler : register(s0);

struct VertexToPixel
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD;
};

float4 main(VertexToPixel input) : SV_TARGET
{
    float3 directColor = directLighting.Sample(basicSampler, input.uv);
    float3 indDiffuse = indirectDiffuse.Sample(basicSampler, input.uv);
    float3 indSpec =    indirectSpecular.Sample(basicSampler, input.uv);

    float3 finalCol = directColor + indDiffuse + indSpec;
    
    return float4(finalCol, 1.0f);
}