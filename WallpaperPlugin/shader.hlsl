typedef struct 
{
	float4 position : SV_Position;
	float2 uv : TEXCOORD0;
} VSOut, PSIn;

VSOut vertex(uint VERTEX_ID : SV_VertexID)
{
	float4 v[] = {
		float4 (-1.0f, -1.0f, 0.0f, 1.0f),
		float4 ( 1.0f,  1.0f, 0.0f, 1.0f),
		float4 ( 1.0f, -1.0f, 0.0f, 1.0f),
		float4 (-1.0f, -1.0f, 0.0f, 1.0f),
		float4 (-1.0f,  1.0f, 0.0f, 1.0f),
		float4 ( 1.0f,  1.0f, 0.0f, 1.0f),
	};
	
	float2 texcoord[] = {
		float2 (0.0f, 0.0f),
		float2 (1.0f, 1.0f),
		float2 (1.0f, 0.0f),
		
		float2 (0.0f, 0.0f),
		float2 (0.0f, 1.0f),
		float2 (1.0f, 1.0f),
	};
	
	VSOut o;
	o.position = v [VERTEX_ID];
	o.uv = texcoord [VERTEX_ID];
	return (o);
}

sampler Sampler : register(s0);
Texture2D Texture : register(t0);

float4 pixel(PSIn v) : SV_Target0
{
	return Texture.Sample (Sampler, v.uv);
}
