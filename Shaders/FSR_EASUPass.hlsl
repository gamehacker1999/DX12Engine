// FidelityFX Super Resolution Sample
//
// Copyright (c) 2021 Advanced Micro Devices, Inc. All rights reserved.
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files(the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and / or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions :
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
cbuffer cb : register(b0)
{
    uint4 Const0;
    uint4 Const1;
    uint4 Const2;
    uint4 Const3;
    uint4 Sample;
};

#define A_GPU 1
#define A_HLSL 1

SamplerState samLinearClamp : register(s0);


#define A_HALF
#include "ffx_a.h"

Texture2D<AH4> InputTexture : register(t0);
RWTexture2D<AH4> OutputTexture : register(u0);

#define FSR_EASU_H 1
AH4 FsrEasuRH(AF2 p) { AH4 res = InputTexture.GatherRed(samLinearClamp, p, int2(0, 0)); return res; }
AH4 FsrEasuGH(AF2 p) { AH4 res = InputTexture.GatherGreen(samLinearClamp, p, int2(0, 0)); return res; }
AH4 FsrEasuBH(AF2 p) { AH4 res = InputTexture.GatherBlue(samLinearClamp, p, int2(0, 0)); return res; }	


#include "ffx_fsr1.h"

void CurrFilter(int2 pos)
{

	AH3 c;
	FsrEasuH(c, pos, Const0, Const1, Const2, Const3);
	if (Sample.x == 1)
		c *= c;
	OutputTexture[pos] = AH4(c, 1);

}

[numthreads(64, 1, 1)]
void main(uint3 LocalThreadId : SV_GroupThreadID, uint3 WorkGroupId : SV_GroupID, uint3 Dtid : SV_DispatchThreadID)
{
	// Do remapping of local xy in workgroup for a more PS-like swizzle pattern.
    AU2 gxy = ARmp8x8(LocalThreadId.x) + AU2(WorkGroupId.x << 4u, WorkGroupId.y << 4u);
    CurrFilter(gxy);
    gxy.x += 8u;
    CurrFilter(gxy);
    gxy.y += 8u;
    CurrFilter(gxy);
    gxy.x -= 8u;
    CurrFilter(gxy);
}