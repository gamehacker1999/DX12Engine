cbuffer externData: register(b0)
{
	int pingpong;
	int N;
}
#define mod(x,y) (x-y*floor(x/y))

RWTexture2D<float4> displacement: register(u0);
RWTexture2D<float4> pingpong1: register(u1);
RWTexture2D<float4> pingpong0: register(u2);

[numthreads(16, 16, 1)]
void main(uint3 id : SV_DispatchThreadID)
{
	uint2 x = id.xy;
	float perms[2] = { 1.0f,-1.0f };
	int index = int(mod((int(x.x + x.y)), 2));
	float perm = perms[index];

	if (pingpong == 0)
	{
		float h = pingpong0[x].r;
		float colorVal = perm * (h / float(N * N));
		displacement[x] = float4(colorVal, colorVal, colorVal, 1.0);

	}

	else if (pingpong == 1)
	{
		float h = pingpong1[x].r;
		float colorVal = perm * (h / float(N * N));
		displacement[x] = float4(colorVal, colorVal, colorVal, 1.0);
	}
}