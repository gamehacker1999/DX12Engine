#define MAX_INDICES 256
#define mod(x,y) (x-y*floor(x/y))
static const float PI = 3.14159f;

struct BitIndices
{
	int index;
	float3 padding;
};

cbuffer externData: register(b0)
{
	BitIndices indices[MAX_INDICES];
	int fftRes;
}



struct Complex
{
	float real;
	float im;
};

RWTexture2D<float4> twiddleIndices: register(u0);

[numthreads(1, 16, 1)]
void main(uint3 id : SV_DispatchThreadID)
{
	float2 x = float2(id.xy);
	float k = mod(x.y * (float(fftRes) / pow(2, x.x + 1)), fftRes);

	Complex twiddle;
	twiddle.real = cos(2.0 * PI * k / float(fftRes));
	twiddle.im = sin(2.0 * PI * k / float(fftRes));

	int butterflySpan = int(pow(2, x.x));
	int butterflywing;
	if (mod(x.y, pow(2, x.x + 1)) < pow(2, x.x))
	{
		butterflywing = 1;
	}

	else
	{
		butterflywing = 0;
	}

	if (x.x == 0)
	{
		//top wing
		if (butterflywing == 1)
		{
			twiddleIndices[x] = float4(twiddle.real, twiddle.im,
				indices[x.y].index, indices[x.y + 1].index);
		}

		else
		{
			twiddleIndices[x] = float4(twiddle.real, twiddle.im,
				indices[x.y - 1].index, indices[x.y].index);
		}
	}

	else
	{
		//top wing
		if (butterflywing == 1)
		{
			twiddleIndices[x] = float4(twiddle.real, twiddle.im,
				x.y, x.y + butterflySpan);
		}

		else
		{
			twiddleIndices[x] = float4(twiddle.real, twiddle.im,
				x.y - butterflySpan, x.y);
		}
	}
}