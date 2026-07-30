struct VertexOutput
{
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
};

VertexOutput main(uint vertexId : SV_VertexID)
{
    VertexOutput output;
    float2 position = float2(vertexId == 2 ? 3.0 : -1.0, vertexId == 1 ? 3.0 : -1.0);
    output.position = float4(position, 0.0, 1.0);
    output.uv = float2((position.x + 1.0) * 0.5, 1.0 - (position.y + 1.0) * 0.5);
    return output;
}
