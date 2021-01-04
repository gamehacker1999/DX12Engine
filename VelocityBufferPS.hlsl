struct VertexToPixel
{
	float4 position : SV_POSITION;
	float4 prevPosition : POS;

};

float2 main(VertexToPixel input) : SV_TARGET
{
	float2 initPos = (input.position.xy / input.position.w) * 0.5 + 0.5;
	initPos.y *= -1;
	float2 prevPos = (input.prevPosition.xy / input.prevPosition.w) * 0.5 + 0.5;
	prevPos.y *= -1;
	
    return (initPos - prevPos);
}