/*===============================================================================
Copyright (c) 2023 PTC Inc. and/or Its Subsidiary Companies. All Rights Reserved.

Vuforia is a trademark of PTC Inc., registered in the United States and other
countries.
===============================================================================*/

#pragma once

#include "DeviceResources.h"

#include <VuforiaEngine/VuforiaEngine.h>

#include <d3d11.h>
#include <wincodec.h>

namespace SampleCommon
{
/// Handle loading textures and preparing them for rendering with DirectX.
class Texture
{
public:
    Texture(const std::shared_ptr<winrt::DX::DeviceResources>& deviceResources);
    ~Texture();

    void CreateFromFile(const wchar_t* filename);
    void CreateFromVuforiaImage(const VuImageInfo& image);

    void Init();
    void ReleaseResources();

    bool IsInitialized() const { return mInitialized; }
    winrt::com_ptr<ID3D11SamplerState>& GetD3DSamplerState() { return mSamplerState; }
    winrt::com_ptr<ID3D11ShaderResourceView>& GetD3DTextureView() { return mTextureView; }
    winrt::com_ptr<ID3D11Texture2D>& GetD3DTexture() { return mTexture; }

private: // methods
    void CreateTexture();

private: // data members
    // Cached pointer to device resources.
    std::shared_ptr<winrt::DX::DeviceResources> mDeviceResources;

    winrt::com_ptr<ID3D11Texture2D> mTexture;
    winrt::com_ptr<ID3D11SamplerState> mSamplerState;
    winrt::com_ptr<ID3D11ShaderResourceView> mTextureView;

    winrt::com_ptr<IWICImagingFactory> mImagingFactory;
    winrt::com_ptr<IWICFormatConverter> mFormatConverter;

    UINT mImageWidth;
    UINT mImageHeight;
    size_t mRowPitch;
    size_t mImageSize;
    std::unique_ptr<uint8_t[]> mImageBytes;
    uint8_t* mImageBytePtr;

    bool mInitialized;
};
} // namespace SampleCommon
