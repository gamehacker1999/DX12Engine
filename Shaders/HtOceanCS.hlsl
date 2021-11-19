#include "Common.hlsl"

cbuffer externData: register(b0)
{
	int fftRes;
	int L;
	float time;
}


RWTexture2D<float4> tildeH0: register(u0);
RWTexture2D<float4> tildeMinusH0: register(u1);

RWTexture2D<float4> tildeHktDx: register(u2);
RWTexture2D<float4> tildeHktDy: register(u3);
RWTexture2D<float4> tildeHktDz: register(u4);

static const float PI = 3.1415926535897932384626433832795f;
static const float g = 9.81f;

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

[numthreads(16, 16, 1)]
void main(uint3 id : SV_DispatchThreadID)
{
	float2 x = uint2(id.xy) - float(fftRes) / 2.0f;
	float2 k = float2(2 * PI * x.x / L, 2 * PI * x.y / L);
	float mag = length(k);
	if (mag < 0.00001f) mag = 0.00001;
	float magSq = mag * mag;

	float w = sqrt(9.81 * mag);

	float2 tildeH0Vals = tildeH0[id.xy].rg;
	Complex fourierCmp;
	fourierCmp.real = tildeH0Vals.x;
	fourierCmp.im = tildeH0Vals.y;

	float2 tildeH0MinusVals = tildeMinusH0[id.xy].rg;
	Complex fourierCmpConj;
	//= Conj(Complex(tildeH0MinusVals.x, tildeH0MinusVals.y));
	fourierCmpConj.real = tildeH0MinusVals.x;
	fourierCmpConj.im = tildeH0MinusVals.y;
	fourierCmpConj = Conj(fourierCmpConj);

	float cosWT = cos(w * time);
	float sinWT = sin(w * time);

	//using euler formula e^it = cos(t)+isin(t)
	Complex expIWT;
	expIWT.real = cosWT;
	expIWT.im = sinWT;
	//= Complex(cosWT, sinWT);
	Complex expIWTInv;
	expIWTInv.real = cosWT;
	expIWTInv.im = -sinWT;
	//= Complex(cosWT, -sinWT);

	Complex hkdy = Add(Mul(fourierCmp, expIWT), Mul(fourierCmpConj, expIWTInv));

	Complex dx;
	dx.real = 0.0f;
	dx.im = -k.x / mag;
	//= Complex(0.0, -k.x / mag);
	Complex hkdx = Mul(dx, hkdy);

	Complex dz;
	dz.real = 0.0f;
	dz.im = -k.y / mag;
	//= Complex(0.0, -k.y / mag);
	Complex hkdz = Mul(dz, hkdy);


	tildeHktDx[id.xy] = float4(hkdx.real, hkdx.im, 0, 1.0);
	tildeHktDy[id.xy] = float4(hkdy.real, hkdy.im, 0, 1.0);
	tildeHktDz[id.xy] = float4(hkdz.real, hkdz.im, 0, 1.0);

}