/*===============================================================================
Copyright (c) 2023 PTC Inc. and/or Its Subsidiary Companies. All Rights Reserved.

Vuforia is a trademark of PTC Inc., registered in the United States and other
countries.
===============================================================================*/
struct PixelShaderInput
{
    float4 pos : SV_POSITION;
    float2 texcoord : TEXCOORD0;
};

Texture2D Texture : register(t0);
sampler Sampler : register(s0);

float4 main(PixelShaderInput input) : SV_TARGET
{
    return Texture.Sample(Sampler, input.texcoord);
}
