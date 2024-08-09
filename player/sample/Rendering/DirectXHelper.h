/*===============================================================================
Copyright (c) 2023 PTC Inc. and/or Its Subsidiary Companies. All Rights Reserved.

Vuforia is a trademark of PTC Inc., registered in the United States and other
countries.
===============================================================================*/

#pragma once

#include <d3d11.h>
#include <pplawait.h> // For concurrency types on winrt
#include <winrt/Windows.ApplicationModel.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Storage.Streams.h>
#include <winrt/Windows.Storage.h>


namespace winrt::DX
{
// Function that reads from a binary file asynchronously.
inline concurrency::task<std::vector<byte>>
ReadDataAsync(const hstring& filename)
{
    using namespace Windows::Storage;

    auto folder = Windows::ApplicationModel::Package::Current().InstalledLocation();

    auto file = co_await folder.GetFileAsync(filename);

    auto fileBuffer = co_await FileIO::ReadBufferAsync(file);

    std::vector<byte> returnBuffer;
    returnBuffer.resize(fileBuffer.Length());
    Streams::DataReader::FromBuffer(fileBuffer).ReadBytes(returnBuffer);

    co_return returnBuffer;
}


// Converts a length in device-independent pixels (DIPs) to a length in physical pixels.
inline float
ConvertDipsToPixels(float dips, float dpi)
{
    static const float dipsPerInch = 96.0f;
    return floorf(dips * dpi / dipsPerInch + 0.5f); // Round to nearest integer.
}


inline D3D11_BLEND_DESC
CreateBlendDesc(bool transparent)
{
    D3D11_BLEND_DESC blendDesc;
    ZeroMemory(&blendDesc, sizeof(D3D11_BLEND_DESC));
    blendDesc.RenderTarget[0].BlendEnable = transparent;
    blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
    blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
    blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_INV_DEST_ALPHA;
    blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ONE;
    blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
    blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    return blendDesc;
}


inline D3D11_RASTERIZER_DESC
CreateRasterizerDesc(D3D11_FILL_MODE fillMode, D3D11_CULL_MODE cullMode, bool frontFaceCounterClockwise, bool depthClipping)
{
    D3D11_RASTERIZER_DESC rasterDesc;
    rasterDesc.AntialiasedLineEnable = false;
    rasterDesc.CullMode = cullMode; // culling
    rasterDesc.DepthBias = 0;
    rasterDesc.DepthBiasClamp = 0.0f;
    rasterDesc.DepthClipEnable = depthClipping;                   // depth clipping
    rasterDesc.FillMode = fillMode;                               // solid or wireframe
    rasterDesc.FrontCounterClockwise = frontFaceCounterClockwise; // CCW or CW front face
    rasterDesc.MultisampleEnable = false;
    rasterDesc.ScissorEnable = false;
    rasterDesc.SlopeScaledDepthBias = 0.0f;
    return rasterDesc;
}


inline D3D11_DEPTH_STENCIL_DESC
CreateDepthStencilDesc(bool depthEnabled, D3D11_DEPTH_WRITE_MASK depthWriteMask, D3D11_COMPARISON_FUNC depthCompareFunc)
{
    D3D11_DEPTH_STENCIL_DESC depthStencilDesc;
    depthStencilDesc.DepthEnable = depthEnabled;
    depthStencilDesc.DepthWriteMask = depthWriteMask;
    depthStencilDesc.DepthFunc = depthCompareFunc;

    // Stencil test parameters
    depthStencilDesc.StencilEnable = false;
    depthStencilDesc.StencilReadMask = 0xFF;
    depthStencilDesc.StencilWriteMask = 0xFF;

    // Stencil operations if pixel is front-facing
    depthStencilDesc.FrontFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
    depthStencilDesc.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_INCR;
    depthStencilDesc.FrontFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
    depthStencilDesc.FrontFace.StencilFunc = D3D11_COMPARISON_ALWAYS;

    // Stencil operations if pixel is back-facing
    depthStencilDesc.BackFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
    depthStencilDesc.BackFace.StencilDepthFailOp = D3D11_STENCIL_OP_DECR;
    depthStencilDesc.BackFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
    depthStencilDesc.BackFace.StencilFunc = D3D11_COMPARISON_ALWAYS;

    return depthStencilDesc;
}


#if defined(_DEBUG)
// Check for SDK Layer support.
inline bool
SdkLayersAvailable()
{
    HRESULT hr = D3D11CreateDevice(nullptr,
                                   D3D_DRIVER_TYPE_NULL, // There is no need to create a real hardware device.
                                   0,
                                   D3D11_CREATE_DEVICE_DEBUG, // Check for the SDK layers.
                                   nullptr,                   // Any feature level will do.
                                   0,
                                   D3D11_SDK_VERSION, // Always set this to D3D11_SDK_VERSION for Windows Store apps.
                                   nullptr,           // No need to keep the D3D device reference.
                                   nullptr,           // No need to know the feature level.
                                   nullptr            // No need to keep the D3D device context reference.
    );

    return SUCCEEDED(hr);
}
#endif
}
