static const float PI = 3.14159f;

cbuffer externData: register(b0)
{
	int stage;
	int pingpong;
	int direction;
}

struct Complex
{
	float real;
	float im;
};

Complex Mul(Complex c0, Complex c1)
{
	Complex c;
	c.real = c0.real * c1.real - c0.im * c1.im;
	c.im = c0.real * c1.im + c0.im * c1.real;

	return c;
}

Complex Add(Complex c0, Complex c1)
{
	Complex c;
	c.real = c1.real + c0.real;
	c.im = c1.im + c0.im;
	return c;
}

Complex Conj(Complex c0)
{
	Complex c;
	c.real = c0.real;
	c.im = -c0.im;
	return c;
}

RWTexture2D<float4> twiddleIndices: register(u0);
RWTexture2D<float4> pingpong1: register(u1);
RWTexture2D<float4> pingpong0: register(u2);

void HorizontalButterflies(uint3 id)
{
	Complex H;
	uint2 x = id.xy;

	if (pingpong == 0)
	{
		float4 data = twiddleIndices[uint2(stage, x.x)].rgba;
		float2 p_ = pingpong0[uint2(data.z, x.y)].rg;
		float2 q_ = pingpong0[uint2(data.w, x.y)].rg;
		float2 w_ = float2(data.x, data.y);

		Complex p;
		p.real = p_.x; p.im = p_.y;
		Complex q;
		q.real = q_.x; q.im = q_.y;
		Complex w;
		w.real = w_.x; w.im = w_.y;

		H = Add(p, Mul(w, q));
		pingpong1[x] = float4(H.real, H.im, 0, 1);
	}

	if (pingpong == 1)
	{
		float4 data = twiddleIndices[uint2(stage, x.x)].rgba;
		float2 p_ = pingpong1[uint2(data.z, x.y)].rg;
		float2 q_ = pingpong1[uint2(data.w, x.y)].rg;
		float2 w_ = float2(data.x, data.y);

		Complex p;
		p.real = p_.x; p.im = p_.y;
		Complex q;
		q.real = q_.x; q.im = q_.y;
		Complex w;
		w.real = w_.x; w.im = w_.y;

		H = Add(p, Mul(w, q));
		pingpong0[x] = float4(H.real, H.im, 0, 1);
	}
}

void VerticalButterflies(uint3 id)
{
	Complex H;
	uint2 x = id.xy;

	if (pingpong == 0)
	{
		float4 data = twiddleIndices[uint2(stage, x.y)].rgba;
		float2 p_ = pingpong0[uint2(x.x, data.z)].rg;
		float2 q_ = pingpong0[uint2(x.x, data.w)].rg;
		float2 w_ = float2(data.x, data.y);

		Complex p;
		p.real = p_.x; p.im = p_.y;
		Complex q;
		q.real = q_.x; q.im = q_.y;
		Complex w;
		w.real = w_.x; w.im = w_.y;

		H = Add(p, Mul(w, q));
		pingpong1[x] = float4(H.real, H.im, 0, 1);
	}

	if (pingpong == 1)
	{
		float4 data = twiddleIndices[uint2(stage, x.y)].rgba;
		float2 p_ = pingpong1[uint2(x.x, data.z)].rg;
		float2 q_ = pingpong1[uint2(x.x, data.w)].rg;
		float2 w_ = float2(data.x, data.y);

		Complex p;
		p.real = p_.x; p.im = p_.y;
		Complex q;
		q.real = q_.x; q.im = q_.y;
		Complex w;
		w.real = w_.x; w.im = w_.y;

		H = Add(p, Mul(w, q));
		pingpong0[x] = float4(H.real, H.im, 0, 1);
	}
}

[numthreads(16, 16, 1)]
void main(uint3 id : SV_DispatchThreadID)
{
	if (direction == 0)
		HorizontalButterflies(id);
	else if (direction == 1)
		VerticalButterflies(id);
}