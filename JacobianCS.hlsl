RWTexture2D<float4> heightMapDX: register(u1);
RWTexture2D<float4> heightMapDY: register(u2);
RWTexture2D<float4> foldingMap: register(u3);

[numthreads(16, 16, 1)]
void main(uint3 id : SV_DispatchThreadID)
{
	uint2 x = uint2(id.xy);

	uint2 dy = uint2(x.x, x.y + 1);
	uint2 dx = uint2(x.x + 1, x.y);

	float y1 = heightMapDY[x].r;
	float y2 = heightMapDY[dy].r;
	float y3 = heightMapDY[dx].r;

	float x1 = heightMapDX[x].r;
	float x2 = heightMapDX[dx].r;
	float x3 = heightMapDX[dy].r;

	float jyy = 1 + ((y2 - y1) / length(dy - x)) * 1.8;
	float jxx = 1 + ((x2 - x1) / length(dx - x)) * 1.8;

	float jxy = 1 + ((x3 - y1) / length(dy - x)) * 1.8;
	float jyx = 1 + ((y3 - y1) / length(dx - x)) * 1.8;

	float j = (jxx * jyy) - (jxy * jyx);

	float m = 0.1f;

	//j = saturate(j);
	foldingMap[x] = float4(j + m, j + m, j + m, 1.0);
}