/*===============================================================================
Copyright (c) 2023 PTC Inc. and/or Its Subsidiary Companies. All Rights Reserved.

Vuforia is a trademark of PTC Inc., registered in the United States and other
countries.
===============================================================================*/
cbuffer ProjectionConstantBuffer : register(b0)
{
    matrix projection;
};

struct VertexShaderInput
{
    float3 pos : POSITION;
    float2 texcoord : TEXCOORD0;
};

struct PixelShaderInput
{
    float4 pos : SV_POSITION;
    float2 texcoord : TEXCOORD0;
};

PixelShaderInput main(VertexShaderInput input)
{
    PixelShaderInput output;
    float4 pos = float4(input.pos, 1.0f);

    // Transform the vertex position into projected space.
    pos = mul(pos, projection);
    output.pos = pos;
    output.texcoord = input.texcoord;
    return output;
}
