/*===============================================================================
Copyright (c) 2023 PTC Inc. and/or Its Subsidiary Companies. All Rights Reserved.

Vuforia is a trademark of PTC Inc., registered in the United States and other
countries.
===============================================================================*/
#pragma once

#include <DirectXColors.h>
#include <DirectXMath.h>

namespace SampleCommon
{
// Constant buffer used to send projection matrices to the vertex shader.
struct ConstColorShaderConstantBuffer
{
    DirectX::XMFLOAT4X4 modelView;
    DirectX::XMFLOAT4X4 projection;
    DirectX::XMFLOAT4 color;
};

// Used to send per-vertex data to the vertex shader.
struct ConstColorShaderInputBuffer
{
    DirectX::XMFLOAT3 pos;
};

// Constant buffer used to send projection matrices to the vertex shader.
struct VertexColorShaderConstantBuffer
{
    DirectX::XMFLOAT4X4 modelView;
    DirectX::XMFLOAT4X4 projection;
};

// Used to send per-vertex data to the vertex shader.
struct VertexColorShaderInputBuffer
{
    DirectX::XMFLOAT3 pos;
    DirectX::XMFLOAT3 color;
};

// Constant buffer used to send projection matrices to the vertex shader.
struct TexturedShaderConstantBuffer
{
    DirectX::XMFLOAT4X4 modelView;
    DirectX::XMFLOAT4X4 projection;
};

// Used to send per-vertex data to the vertex shader.
struct TexturedShaderInputBuffer
{
    DirectX::XMFLOAT3 pos;
    DirectX::XMFLOAT2 texcoord;
};

// Constant buffer used to send projection matrices to the vertex shader.
struct VideoBackgroundShaderConstantBuffer
{
    DirectX::XMFLOAT4X4 projection;
};

// Used to send per-vertex data to the vertex shader.
struct VideoBackgroundShaderInputBuffer
{
    DirectX::XMFLOAT3 pos;
    DirectX::XMFLOAT2 texcoord;
};
}
