#pragma once
#include<DirectXMath.h>

using namespace DirectX;

void Orthonormalize(XMVECTOR& up, XMVECTOR& forward)
{
    XMVECTOR outdir = forward;

    float dot = XMVector3Dot(forward, up).m128_f32[0];

    float length = XMVector3Dot(outdir, outdir).m128_f32[0];

    XMVECTOR intermediateUp = (dot / length) * outdir;

    XMVECTOR finalUp = up - intermediateUp;

    forward = XMVector3Normalize(outdir);
    up = XMVector3Normalize(finalUp);
}

XMVECTOR QuaternionLookRotation(XMVECTOR forward, XMVECTOR up)
{
    //forward.Normalize();
    forward = XMVector3Normalize(forward);

    Orthonormalize(up, forward);

    XMVECTOR vector = XMVector3Normalize(forward);
    XMVECTOR vector2 = XMVector3Normalize(XMVector3Cross(up, vector));
    XMVECTOR vector3 = XMVector3Cross(vector, vector2);
    auto m00 = vector2.m128_f32[0];
    auto m01 = vector2.m128_f32[1];
    auto m02 = vector2.m128_f32[2];
    auto m10 = vector3.m128_f32[0];
    auto m11 = vector3.m128_f32[1];
    auto m12 = vector3.m128_f32[2];
    auto m20 = vector.m128_f32[0];
    auto m21 = vector.m128_f32[1];
    auto m22 = vector.m128_f32[2];


    float num8 = (m00 + m11) + m22;
    auto quaternion = XMVectorSet(0,0,0,0);
    if (num8 > 0.f)
    {
        auto num = (float)sqrt(num8 + 1.f);
        quaternion.m128_f32[3] = num * 0.5f;
        num = 0.5f / num;
        quaternion.m128_f32[0] = (m12 - m21) * num;
        quaternion.m128_f32[1] = (m20 - m02) * num;
        quaternion.m128_f32[2] = (m01 - m10) * num;
        return quaternion;
    }

    if ((m00 >= m11) && (m00 >= m22))
    {
        auto num7 = (float)sqrt(((1.f + m00) - m11) - m22);
        auto num4 = 0.5f / num7;
        quaternion.m128_f32[0] = 0.5f * num7;
        quaternion.m128_f32[1] = (m01 + m10) * num4;
        quaternion.m128_f32[2] = (m02 + m20) * num4;
        quaternion.m128_f32[3] = (m12 - m21) * num4;
        return quaternion;
    }

    if (m11 > m22)
    {
        auto num6 = (float)sqrt(((1.f + m11) - m00) - m22);
        auto num3 = 0.5f / num6;
        quaternion.m128_f32[0] = (m10 + m01) * num3;
        quaternion.m128_f32[1] = 0.5f * num6;
        quaternion.m128_f32[2] = (m21 + m12) * num3;
        quaternion.m128_f32[3] = (m20 - m02) * num3;
        return quaternion;
    }

    auto num5 = (float)sqrt(((1.f + m22) - m00) - m11);
    auto num2 = 0.5f / num5;
    quaternion.m128_f32[0] = (m20 + m02) * num2;
    quaternion.m128_f32[1] = (m21 + m12) * num2;
    quaternion.m128_f32[2] = 0.5f * num5;
    quaternion.m128_f32[3] = (m01 - m10) * num2;
    return quaternion;
}

