struct Ray
{
	float3 origin;
	float3 direction;
};

struct AABB
{
	float3 min;
	float3 max;
};

struct VertexToPixel
{
	float4 position: SV_POSITION;
	float4 color: COLOR;
	float3 normal:  NORMAL;
	float3 tangent: TANGENT;
	float3 worldPosition: POSITION;
	float2 uv: TEXCOORD;

};																																											

cbuffer VolumeData
{
	matrix view;
};

void RayBoxIntersection(Ray ray, AABB box, out float t0, out float t1)
{
	//calculating the inverse ray
	float3 invRay = 1.0 / ray.direction;
	float3 ttop = invRay * (box.top - ray.origin);
	float3 tbottom = invRay * (box.bottom - ray.origin);
	float3 min = min(ttop.tbottom);
	float2 t = max(min.xx, min.yz);
	t0 = max(0, max(t.x, t.y));
	float3 max = (ttop, tbottom);
	t = min(max.xx, max.yz);
	t1 = min(t.x, t.y);
}

float4 main() : SV_TARGET
{

	float3 rayDirection;
	rayDirection.xy = input.position
	return float4(1.0f, 1.0f, 1.0f, 1.0f);
}