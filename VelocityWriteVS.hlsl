struct CameraConstantBuffer
{
    matrix view;
    matrix projection;
    matrix world;
    matrix worldInvTranspose;
    matrix prevView;
    matrix prevProjection;
    matrix prevWorld;
};

ConstantBuffer<CameraConstantBuffer> sceneData : register(b1);

float4 main( float4 pos : POSITION ) : SV_POSITION
{
	return pos;
}