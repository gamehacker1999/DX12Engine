/*
Copyright (c) 2021, NVIDIA CORPORATION. All rights reserved.

NVIDIA CORPORATION and its licensors retain all intellectual property
and proprietary rights in and to this software, related documentation
and any modifications thereto. Any use, reproduction, disclosure or
distribution of this software and related documentation without an express
license agreement from NVIDIA CORPORATION is strictly prohibited.
*/

#ifndef REBLUR_SPATIAL_MODE
    #error REBLUR_SPATIAL_MODE must be defined!
#endif

{
    float4 center = spec;

    float exposure = _NRD_GetColorCompressionExposureForSpatialPasses( roughness );
    spec.xyz = STL::Color::Compress( spec.xyz, exposure );

    float radius = gSpecBlurRadius;
    #if( REBLUR_SPATIAL_MODE == REBLUR_PRE_BLUR )
        float2 specInternalData = REBLUR_PRE_BLUR_INTERNAL_DATA;
        radius *= REBLUR_PRE_BLUR_RADIUS_SCALE( roughness );
    #else
        float minAccumSpeed = GetMipLevel( roughness, gSpecMaxFastAccumulatedFrameNum ) + 0.001;
        float boost = saturate( 1.0 - specInternalData.y / minAccumSpeed );
        radius *= ( 1.0 + 2.0 * boost ) / 3.0;

        radius *= ( 1.0 + 2.0 * error.z * roughness );
    #endif
    radius *= GetBlurRadiusScaleBasingOnTrimming( roughness, gSpecTrimmingParams.xyz );

    // Blur radius scale
    #if( REBLUR_SPATIAL_MODE == REBLUR_POST_BLUR )
        float radiusScale = REBLUR_POST_BLUR_RADIUS_SCALE;
    #else
        float radiusScale = 1.0;
    #endif

    // TODO: on thin objects adaptive scale can lead to sampling from "not contributing" surfaces. Mips for viewZ can be computed
    // and used to estimate "local planarity". If abs( z_mip0 - z_mip3 ) is big reduce adaptive radius scale accordingly.
    #if( REBLUR_SPATIAL_MODE == REBLUR_PRE_BLUR )
        float radiusBias = 0.0;
    #elif( REBLUR_SPATIAL_MODE == REBLUR_BLUR )
        float radiusBias = gSpecBlurRadiusScale + 0.00001; // TODO: using error.x introduces bias
    #else
        float radiusBias = error.y * gSpecBlurRadiusScale + 0.00001;
    #endif

    // Blur radius
    float hitDist = GetHitDist( center.w, viewZ, gSpecHitDistParams, roughness );
    float blurRadius = GetBlurRadius( radius, roughness, hitDist, viewZ, specInternalData.x, error.z, error.y, radiusBias, radiusScale );
    float worldBlurRadius = PixelRadiusToWorld( gUnproject, gIsOrtho, blurRadius, viewZ );

    // Denoising
    float anisoFade = lerp( edge, 1.0, specInternalData.x );
    float2x3 TvBv = GetKernelBasis( Xv, Nv, worldBlurRadius, roughness, anisoFade );
    float2 geometryWeightParams = GetGeometryWeightParams( gPlaneDistSensitivity, Xv, Nv, lerp( 1.0, 0.05, specInternalData.x ) );
    float normalWeightParams = GetNormalWeightParams( specInternalData.x, edge, error.y, roughness, gNormalWeightStrictness );
    float2 hitDistanceWeightParams = GetHitDistanceWeightParams( center.w, specInternalData.x, roughness );
    float2 roughnessWeightParams = GetRoughnessWeightParamForSpatialPasses( roughness );
    float2 sum = 1.0;

    #if( REBLUR_USE_DOMINANT_DIRECTION_IN_WEIGHT == 1 )
        float dominantFactor = STL::ImportanceSampling::GetSpecularDominantFactor( 0, roughness, STL_SPECULAR_DOMINANT_DIRECTION_APPROX );
        float3 Vv = -normalize( Xv );
        float3 Dv = STL::ImportanceSampling::GetSpecularDominantDirectionWithFactor( Nv, Vv, dominantFactor );
    #endif

    [unroll]
    for( uint i = 0; i < REBLUR_POISSON_SAMPLE_NUM; i++ )
    {
        float3 offset = REBLUR_POISSON_SAMPLES[ i ];

        // Sample coordinates
        float2 uv = GetKernelSampleCoordinates( gViewToClip, offset, Xv, TvBv[ 0 ], TvBv[ 1 ], rotator );

        // Handle half res input in the checkerboard mode
        #if( REBLUR_SPATIAL_MODE == REBLUR_PRE_BLUR )
            float2 checkerboardUv = uv;
            if( gSpecCheckerboard != 2 )
                checkerboardUv = ApplyCheckerboard( uv, gSpecCheckerboard, i, gRectSize, gInvRectSize, gFrameIndex );
        #endif

        // Fetch data
        float2 uvScaled = uv * gResolutionScale;
        float4 Ns = gIn_Normal_Roughness.SampleLevel( gNearestMirror, uvScaled + gRectOffset, 0 );

        #if( REBLUR_SPATIAL_MODE == REBLUR_PRE_BLUR )
            float2 checkerboardUvScaled = checkerboardUv * gResolutionScale + gRectOffset;
            float4 s = gIn_Spec.SampleLevel( gNearestMirror, checkerboardUvScaled, 0 );
            float zs = abs( gIn_ViewZ.SampleLevel( gNearestMirror, uvScaled + gRectOffset, 0 ) );
        #else
            float4 s = gIn_Spec.SampleLevel( gNearestMirror, uvScaled, 0 );
            float zs = gIn_ScaledViewZ.SampleLevel( gNearestMirror, uvScaled, 0 ) / NRD_FP16_VIEWZ_SCALE;
        #endif

        float3 Xvs = STL::Geometry::ReconstructViewPosition( uv, gFrustum, zs, gIsOrtho );
        Ns = _NRD_FrontEnd_UnpackNormalAndRoughness( Ns );

        // Sample weight
        float w = GetGeometryWeight( geometryWeightParams, Nv, Xvs );
        w *= IsInScreen( uv );
        w *= GetGaussianWeight( offset.z );
        w *= GetRoughnessWeight( roughnessWeightParams, Ns.w );

        #if( REBLUR_USE_DOMINANT_DIRECTION_IN_WEIGHT == 1 )
            float3 Nvs = STL::Geometry::RotateVector( gWorldToView, Ns.xyz );
            float3 Vvs = -normalize( Xvs );
            float3 Dvs = STL::ImportanceSampling::GetSpecularDominantDirectionWithFactor( Nvs, Vvs, dominantFactor );
            w *= GetNormalWeight( normalWeightParams, Dv, Dvs );
        #else
            w *= GetNormalWeight( normalWeightParams, N, Ns.xyz );
        #endif

        #if( REBLUR_SPATIAL_MODE == REBLUR_PRE_BLUR )
            float2 minwh = REBLUR_HIT_DIST_MIN_WEIGHT * 2.0;
        #elif( REBLUR_SPATIAL_MODE == REBLUR_BLUR )
            float2 minwh = float2( 0.0, REBLUR_HIT_DIST_MIN_WEIGHT );
        #else
            float2 minwh = float2( 0.0, REBLUR_HIT_DIST_MIN_WEIGHT * 0.5 );
        #endif

        float wh = GetHitDistanceWeight( hitDistanceWeightParams, s.w );
        float2 ww = w * lerp( minwh, 1.0, wh );

        s.xyz = STL::Color::Compress( s.xyz, exposure );

        spec += s * ww.xxxy;
        sum += ww;
    }

    spec /= sum.xxxy;

    spec.xyz = STL::Color::Decompress( spec.xyz, exposure );

    // Estimate error
    error.y = GetColorErrorForAdaptiveRadiusScale( spec, center, specInternalData.x, 1.0 ); // TODO: yes, "1" not "roughness"...

    // Input mix
    spec = lerp( spec, center, REBLUR_INPUT_MIX.xxxy );

    // Output
    gOut_Spec[ pixelPos ] = spec;
}
