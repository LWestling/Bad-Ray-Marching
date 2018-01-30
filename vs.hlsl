struct VS_OUT {
    float4 pos : SV_POSITION;
    float4 test : POSITION;
};

VS_OUT main( in float3 pos : POSITION, in float2 uv : UV)
{
    VS_OUT vo;
    vo.pos = float4(pos.xyz, 1.f);
    vo.test = vo.pos;
    return vo;
}