
struct VertexToPixel
{
	float4 position:  SV_POSITION;
	float2 uv:		  TEXCOORD;
};

VertexToPixel main(uint id: SV_VERTEXID)
{
	VertexToPixel output;

	//getting the uv coordinates of the triangle
	output.uv = float2((id << 1) & 2, id & 2);

	//converting them to screen space (-1,-1) to (-3,-3)
	output.position = float4(output.uv, 0.0, 1.0);
	output.position.x = output.position.x * 2 - 1;
	output.position.y = output.position.y * (-2) + 1;

	return output;

} 