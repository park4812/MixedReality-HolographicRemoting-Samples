/*===============================================================================
Copyright (c) 2023 PTC Inc. and/or Its Subsidiary Companies. All Rights Reserved.

Vuforia is a trademark of PTC Inc., registered in the United States and other
countries.
===============================================================================*/

#include "pch.h"

#include "Texture.h"

#include "DirectXHelper.h"

#include <Log.h>

#include <iostream>
#include <memory>


namespace SampleCommon
{
Texture::Texture(const std::shared_ptr<winrt::DX::DeviceResources>& deviceResources) :
    mDeviceResources(deviceResources),
    mTexture(nullptr),
    mInitialized(false),
    mImageWidth(0),
    mImageHeight(0),
    mRowPitch(0),
    mImageSize(0),
    mImageBytePtr(nullptr)
{
    LOG("Texture::Texture() called.");
}


Texture::~Texture()
{
    LOG("Texture::~Texture() called.");

    ReleaseResources();
}


void
Texture::CreateFromFile(const wchar_t* filename)
{
    LOG("Texture::CreateFromFile() called.");

    winrt::com_ptr<IWICBitmapDecoder> decoder;
    if (nullptr == mImagingFactory)
    {
        // Create the ImagingFactory
        winrt::check_hresult(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(mImagingFactory.put())));
    }

    winrt::check_hresult(
        mImagingFactory->CreateDecoderFromFilename(filename, NULL, GENERIC_READ, WICDecodeMetadataCacheOnDemand, decoder.put()));

    // Retrieve the first frame of the image from the decoder
    winrt::com_ptr<IWICBitmapFrameDecode> frame;
    winrt::check_hresult(decoder->GetFrame(0, frame.put()));

    winrt::check_hresult(mImagingFactory->CreateFormatConverter(mFormatConverter.put()));

    winrt::check_hresult(mFormatConverter->Initialize(frame.get(),                  // Input bitmap to convert
                                                      GUID_WICPixelFormat32bppBGRA, // Destination pixel format
                                                      WICBitmapDitherTypeNone, nullptr,
                                                      0.f, // Alpha threshold
                                                      WICBitmapPaletteTypeCustom));

    winrt::check_hresult(mFormatConverter->GetSize(&mImageWidth, &mImageHeight));

    // Allocate temporary memory for image
    mRowPitch = mImageWidth * size_t(4); // 4 bytes per pixel (32bit RGBA)
    mImageSize = mRowPitch * mImageHeight;

    mImageBytes = std::unique_ptr<uint8_t[]>(new (std::nothrow) uint8_t[mImageSize]);
    winrt::check_hresult(mFormatConverter->CopyPixels(0, static_cast<UINT>(mRowPitch), static_cast<UINT>(mImageSize), mImageBytes.get()));

    mImageBytePtr = mImageBytes.get();

    CreateTexture();
}


void
Texture::CreateFromVuforiaImage(const VuImageInfo& image)
{
    mImageWidth = image.width;
    mImageHeight = image.height;
    mRowPitch = image.stride;
    mImageSize = size_t(image.bufferHeight) * image.bufferWidth;
    mImageBytePtr = (uint8_t*)image.buffer;

    CreateTexture();
}


void
Texture::Init()
{
    LOG("Texture::Init() called.");

    if (mTexture != nullptr)
    {
        mDeviceResources->GetD3DDeviceContext()->UpdateSubresource(mTexture.get(), 0, nullptr, mImageBytePtr, static_cast<UINT>(mRowPitch),
                                                                   static_cast<UINT>(mImageSize));

        mDeviceResources->GetD3DDeviceContext()->GenerateMips(mTextureView.get());
    }
    mInitialized = true;
}


void
Texture::ReleaseResources()
{
    LOG("Texture::ReleaseResources() called.");

    // free unique_ptr's
    mImageBytes.reset();
    mDeviceResources.reset();

    // Free ComPtr's
    mImagingFactory = nullptr;
    mFormatConverter = nullptr;
    mSamplerState = nullptr;
    mTextureView = nullptr;
    mTexture = nullptr;
}


void
Texture::CreateTexture()
{
    D3D11_TEXTURE2D_DESC texDesc;
    ZeroMemory(&texDesc, sizeof(D3D11_TEXTURE2D_DESC));
    texDesc.Width = mImageWidth;
    texDesc.Height = mImageHeight;
    texDesc.MipLevels = 0;
    texDesc.ArraySize = 1;
    texDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    texDesc.SampleDesc.Count = 1;
    texDesc.SampleDesc.Quality = 0;
    texDesc.Usage = D3D11_USAGE_DEFAULT;
    texDesc.CPUAccessFlags = 0;
    texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
    texDesc.MiscFlags = D3D11_RESOURCE_MISC_GENERATE_MIPS;

    winrt::check_hresult(mDeviceResources->GetD3DDevice()->CreateTexture2D(&texDesc, nullptr, mTexture.put()));

    if (mTexture != nullptr)
    {
        D3D11_SHADER_RESOURCE_VIEW_DESC SRVDesc;
        memset(&SRVDesc, 0, sizeof(SRVDesc));
        SRVDesc.Format = texDesc.Format;
        SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        SRVDesc.Texture2D.MipLevels = UINT(-1);

        winrt::check_hresult(mDeviceResources->GetD3DDevice()->CreateShaderResourceView(mTexture.get(), &SRVDesc, mTextureView.put()));
    }

    // Create a texture sampler state description.
    D3D11_SAMPLER_DESC samplerDesc;
    ZeroMemory(&samplerDesc, sizeof(D3D11_SAMPLER_DESC));
    samplerDesc.Filter = D3D11_FILTER_MIN_POINT_MAG_MIP_LINEAR;
    samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
    samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
    samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
    samplerDesc.MipLODBias = 0.0f;
    samplerDesc.MaxAnisotropy = 1;
    samplerDesc.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
    samplerDesc.BorderColor[0] = 0;
    samplerDesc.BorderColor[1] = 0;
    samplerDesc.BorderColor[2] = 0;
    samplerDesc.BorderColor[3] = 0;
    samplerDesc.MinLOD = 0;
    samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;

    winrt::check_hresult(mDeviceResources->GetD3DDevice()->CreateSamplerState(&samplerDesc, mSamplerState.put()));
}
}
