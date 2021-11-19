/*
Copyright (c) 2020, NVIDIA CORPORATION. All rights reserved.

NVIDIA CORPORATION and its licensors retain all intellectual property
and proprietary rights in and to this software, related documentation
and any modifications thereto. Any use, reproduction, disclosure or
distribution of this software and related documentation without an express
license agreement from NVIDIA CORPORATION is strictly prohibited.
*/

struct Payload
{
    float3 hitValue;
};

[shader( "miss" )]
void miss( inout Payload payload : SV_RayPayload )
{
    payload.hitValue = float3( 0.4, 0.3, 0.35 );
}
