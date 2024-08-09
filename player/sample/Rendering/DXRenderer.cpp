/*===============================================================================
Copyright (c) 2023 PTC Inc. and/or Its Subsidiary Companies. All Rights Reserved.

Vuforia is a trademark of PTC Inc., registered in the United States and other
countries.
===============================================================================*/

#include "pch.h"

#include "DXRenderer.h"
#include "DirectXHelper.h"
#include "Texture.h"

#include <Log.h>
#include <MemoryStream.h>
#include <Models.h>

#include <DirectXMath.h>

using namespace winrt;


namespace
{
const winrt::hstring RES_PATH_SHADER_CONST_COLOR_VS = L"ConstColorVertexShader.cso";
const winrt::hstring RES_PATH_SHADER_CONST_COLOR_PS = L"ConstColorPixelShader.cso";
const winrt::hstring RES_PATH_SHADER_VERTEX_COLOR_VS = L"VertexColorVertexShader.cso";
const winrt::hstring RES_PATH_SHADER_VERTEX_COLOR_PS = L"VertexColorPixelShader.cso";
const winrt::hstring RES_PATH_SHADER_TEXTURED_VS = L"TexturedVertexShader.cso";
const winrt::hstring RES_PATH_SHADER_TEXTURED_PS = L"TexturedPixelShader.cso";
const winrt::hstring RES_PATH_SHADER_VIDEO_BKGD_VS = L"VideoBackgroundVertexShader.cso";
const winrt::hstring RES_PATH_SHADER_VIDEO_BKGD_PS = L"VideoBackgroundPixelShader.cso";

const winrt::hstring RES_PATH_ASTRONAUT_MODEL = L"Astronaut.obj";
const winrt::hstring RES_PATH_ASTRONAUT_TEXTURE = L"Astronaut.jpg";
const winrt::hstring RES_PATH_LANDER_MODEL = L"VikingLander.obj";
const winrt::hstring RES_PATH_LANDER_TEXTURE = L"VikingLander.jpg";

const unsigned int NUM_GUIDE_VIEW_VERTEX = 6;

const float WORLDORIGIN_AXES_SCALE = 0.1f;
const float WORLDORIGIN_CUBE_SCALE = 0.015f;
}

namespace SampleColors
{
const DirectX::XMFLOAT4 RED_ALPHA10(1.0f, 0.0f, 0.0f, 0.1f);
}


namespace winrt::VuforiaSample::implementation
{
DXRenderer::DXRenderer(const std::shared_ptr<winrt::DX::DeviceResources>& deviceResources) :
    mDeviceResources(deviceResources),
    mRendererInitialized(false),
    mVideoBackgroundMeshInitialized(false),
    mVideoBackgroundTextureInitialized(false)
{
    CreateDeviceDependentResources();
    CreateWindowSizeDependentResources();
}


DXRenderer::~DXRenderer()
{
    ReleaseDeviceDependentResources();
}

void
DXRenderer::CreateDeviceDependentResources()
{
    LOG("CreateDeviceDependentResources");

    mRendererInitialized = false;

    // Load shaders asynchronously.
    auto loadConstColorVSTask = DX::ReadDataAsync(RES_PATH_SHADER_CONST_COLOR_VS);
    auto loadConstColorPSTask = DX::ReadDataAsync(RES_PATH_SHADER_CONST_COLOR_PS);
    auto loadVertexColorVSTask = DX::ReadDataAsync(RES_PATH_SHADER_VERTEX_COLOR_VS);
    auto loadVertexColorPSTask = DX::ReadDataAsync(RES_PATH_SHADER_VERTEX_COLOR_PS);
    auto loadTexturedVSTask = DX::ReadDataAsync(RES_PATH_SHADER_TEXTURED_VS);
    auto loadTexturedPSTask = DX::ReadDataAsync(RES_PATH_SHADER_TEXTURED_PS);
    auto loadVideoBgVSTask = DX::ReadDataAsync(RES_PATH_SHADER_VIDEO_BKGD_VS);
    auto loadVideoBgPSTask = DX::ReadDataAsync(RES_PATH_SHADER_VIDEO_BKGD_PS);

    // After the vertex shader file is loaded, create the shader and input layout.
    auto createConstColorVSTask =
        loadConstColorVSTask.then([this](const std::vector<byte>& fileData) { initConstColorVertexShader(fileData); });
    auto createVertexColorVSTask =
        loadVertexColorVSTask.then([this](const std::vector<byte>& fileData) { initVertexColorVertexShader(fileData); });
    auto createTexturedVSTask = loadTexturedVSTask.then([this](const std::vector<byte>& fileData) { initTexturedVertexShader(fileData); });
    auto createVideoBgVSTask =
        loadVideoBgVSTask.then([this](const std::vector<byte>& fileData) { initVideoBackgroundVertexShader(fileData); });

    // After the pixel shader file is loaded, create the shader and constant buffer.
    auto createConstColorPSTask =
        loadConstColorPSTask.then([this](const std::vector<byte>& fileData) { initConstColorPixelShader(fileData); });
    auto createVertexColorPSTask =
        loadVertexColorPSTask.then([this](const std::vector<byte>& fileData) { initVertexColorPixelShader(fileData); });
    auto createTexturedPSTask = loadTexturedPSTask.then([this](const std::vector<byte>& fileData) { initTexturedPixelShader(fileData); });
    auto createVideoBgPSTask =
        loadVideoBgPSTask.then([this](const std::vector<byte>& fileData) { initVideoBackgroundPixelShader(fileData); });

    // Load shaders in parallel then setup object-specific rendering
    auto createAugmentationModelsTask =
        (createConstColorPSTask && createConstColorVSTask && createVertexColorPSTask && createVertexColorVSTask && createTexturedPSTask &&
         createTexturedVSTask && createVideoBgPSTask && createVideoBgVSTask)
            .then([this]() {
                initSquare();
                initCube();
                initAxis();
                initGuideView();
            });

    auto createTextureTask = createAugmentationModelsTask.then([this]() { initModels(); });

    auto setupRasterizersTask = createTextureTask.then([this]() {
        // setup the rasterizer
        auto context = mDeviceResources->GetD3DDeviceContext();

        ID3D11Device* device;
        context->GetDevice(&device);

        // Init rendering pipeline state for video background
        initVideoBackgroundRenderState();

        // Create the rasterizer for augmentation rendering
        // with back-face culling
        D3D11_RASTERIZER_DESC augmentationRasterDescCullBack = DX::CreateRasterizerDesc(D3D11_FILL_SOLID, // solid rendering
                                                                                        D3D11_CULL_BACK,  // culling
                                                                                        true,             // Counter clockwise front face
                                                                                        true              // depth clipping enabled
        );
        device->CreateRasterizerState(&augmentationRasterDescCullBack, mAugmentationRasterStateCullBack.put());

        // Create Depth-Stencil State with depth testing ON,
        // for augmentation rendering
        D3D11_DEPTH_STENCIL_DESC augmentDepthStencilDesc =
            DX::CreateDepthStencilDesc(true, D3D11_DEPTH_WRITE_MASK_ALL, D3D11_COMPARISON_LESS);
        device->CreateDepthStencilState(&augmentDepthStencilDesc, mAugmentationDepthStencilState.put());

        // Create blend state for augmentation rendering
        D3D11_BLEND_DESC augmentationBlendDesc = DX::CreateBlendDesc(true);
        device->CreateBlendState(&augmentationBlendDesc, mAugmentationBlendState.put());
    });

    setupRasterizersTask.then([this](Concurrency::task<void> t) {
        try
        {
            // If any exceptions were thrown back in the async chain then
            // this call throws that exception here and we can catch it below
            t.get();

            // Now we are ready for rendering
            mRendererInitialized = true;
        }
        catch (winrt::hresult_error const& ex)
        {
            LOG("Exception setting up rendering (0x%x): %S", uint32_t(ex.code()), ex.message().c_str());
            throw ex;
        }
    });
}


void
DXRenderer::CreateWindowSizeDependentResources()
{
    LOG("CreateWindowSizeDependentResources");
}


void
DXRenderer::ReleaseDeviceDependentResources()
{
    mRendererInitialized = false;

    // Video background
    mVideoBackgroundMeshInitialized = false;
    mVideoBackgroundTextureInitialized = false;

    mVBRasterStateCounterClockwise = nullptr;
    mVBDepthStencilState = nullptr;
    mVBBlendState = nullptr;

    m_vbInputLayout = nullptr;
    m_vbVertexShader = nullptr;
    m_vbPixelShader = nullptr;
    m_vbConstantBuffer = nullptr;
    m_vbVertexBuffer = nullptr;
    mVBIndexBuffer = nullptr;

    mVBTexture = nullptr;
    mVBSamplerState = nullptr;
    mVBTextureView = nullptr;

    // Augmentation
    mAugmentationRasterStateCullBack = nullptr;
    mAugmentationDepthStencilState = nullptr;
    mAugmentationBlendState = nullptr;

    mConstColorInputLayout = nullptr;
    mConstColorVertexShader = nullptr;
    mConstColorPixelShader = nullptr;
    mConstColorConstantBuffer = nullptr;

    mVertexColorInputLayout = nullptr;
    mVertexColorVertexShader = nullptr;
    mVertexColorPixelShader = nullptr;
    mVertexColorConstantBuffer = nullptr;

    mTexturedInputLayout = nullptr;
    mTexturedVertexShader = nullptr;
    mTexturedPixelShader = nullptr;
    mTexturedConstantBuffer = nullptr;

    mSquareVertexBuffer = nullptr;
    mSquareSolidIndexBuffer = nullptr;
    mSquareWireframeIndexBuffer = nullptr;

    mCubeVertexBuffer = nullptr;
    mCubeSolidIndexBuffer = nullptr;
    mCubeWireframeIndexBuffer = nullptr;

    mAxesVertexBuffer = nullptr;
    mAxesIndexBuffer = nullptr;

    mGuideViewVertexBuffer = nullptr;
    mGuideViewTexture.reset();

    mAstronautVertexBuffer = nullptr;
    mAstronautVertexCount = -1;
    mAstronautTexture.reset();

    mLanderVertexBuffer = nullptr;
    mLanderVertexCount = -1;
    mLanderTexture.reset();

    mDeviceResources.reset();
}


void
DXRenderer::initVideoBackgroundTexture(size_t width, size_t height)
{
    LOG("initVideoBackgroundTexture");

    m_imageWidth = width;
    m_imageHeight = height;

    // Setup texture descriptor
    D3D11_TEXTURE2D_DESC texDesc;
    ZeroMemory(&texDesc, sizeof(D3D11_TEXTURE2D_DESC));
    texDesc.Width = static_cast<UINT>(m_imageWidth);
    texDesc.Height = static_cast<UINT>(m_imageHeight);
    texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    texDesc.Usage = D3D11_USAGE_DEFAULT; // Resource requires read and write access by the GPU
    texDesc.CPUAccessFlags = 0;          // CPU access is not required
    texDesc.MiscFlags = 0;
    texDesc.MipLevels = 1;        // Most textures contain more than one MIP level.  For simplicity, this sample uses only one.
    texDesc.ArraySize = 1;        // As this will not be a texture array, this parameter is ignored.
    texDesc.SampleDesc.Count = 1; // Don't use multi-sampling.
    texDesc.SampleDesc.Quality = 0;
    texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET; // Allow the texture to be bound as a shade

    // Create the texture
    winrt::check_hresult(mDeviceResources->GetD3DDevice()->CreateTexture2D(&texDesc, nullptr, mVBTexture.put()));

    // Create a shader resource view for the texture
    D3D11_SHADER_RESOURCE_VIEW_DESC SRVDesc;
    memset(&SRVDesc, 0, sizeof(SRVDesc));
    SRVDesc.Format = texDesc.Format;
    SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    SRVDesc.Texture2D.MipLevels = UINT(-1);

    winrt::check_hresult(mDeviceResources->GetD3DDevice()->CreateShaderResourceView(mVBTexture.get(), &SRVDesc, mVBTextureView.put()));

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

    winrt::check_hresult(mDeviceResources->GetD3DDevice()->CreateSamplerState(&samplerDesc, mVBSamplerState.put()));

    mVideoBackgroundTextureInitialized = true;
}


void
DXRenderer::renderVideoBackground(const VuMatrix44F& projectionMatrix, const int numVertices, const float* vertices, const float* texCoords,
                                  const int numTriangles, const unsigned int* indices)
{
    auto context = mDeviceResources->GetD3DDeviceContext();
    auto device = mDeviceResources->GetD3DDevice();

    if (!mVideoBackgroundTextureInitialized)
    {
        return;
    }

    if (!mVideoBackgroundMeshInitialized)
    {
        // Initialize the video background mesh
        initVideoBackgroundMesh(numVertices, vertices, texCoords, numTriangles, indices, device);
    }

    // Setup rendering pipeline for video background rendering
    context->RSSetState(mVBRasterStateCounterClockwise.get()); // Typically when using the rear facing camera

    context->OMSetDepthStencilState(mVBDepthStencilState.get(), 1);
    context->OMSetBlendState(mVBBlendState.get(), NULL, 0xffffffff);

    // Convert the matrix to XMFLOAT4X4 format
    DirectX::XMFLOAT4X4 vbProjectionDX;
    memcpy(vbProjectionDX.m, projectionMatrix.data, sizeof(float) * 16);
    XMStoreFloat4x4(&vbProjectionDX, DirectX::XMMatrixTranspose(XMLoadFloat4x4(&vbProjectionDX)));

    // Convert the XMFLOAT4X4 to XMMATRIX
    auto vbProjectionMatrix = XMLoadFloat4x4(&vbProjectionDX);

    // Set the projection matrix
    SampleCommon::VideoBackgroundShaderConstantBuffer vbConstantBufferData;
    XMStoreFloat4x4(&vbConstantBufferData.projection, vbProjectionMatrix);
    // Prepare the constant buffer to send it to the graphics device.
    context->UpdateSubresource1(m_vbConstantBuffer.get(), 0, NULL, &vbConstantBufferData, 0, 0, 0);

    // Each vertex is one instance of the input buffer struct.
    UINT stride = sizeof(SampleCommon::VideoBackgroundShaderInputBuffer);
    UINT offset = 0;
    auto vertexBufferPtr = m_vbVertexBuffer.get();
    context->IASetVertexBuffers(0, 1, &vertexBufferPtr, &stride, &offset);

    context->IASetIndexBuffer(mVBIndexBuffer.get(),
                              DXGI_FORMAT_R32_UINT, // Each index is one 32-bit unsigned integer.
                              0);

    context->IASetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    context->IASetInputLayout(m_vbInputLayout.get());

    // Attach our vertex shader.
    context->VSSetShader(m_vbVertexShader.get(), nullptr, 0);

    // Send the constant buffer to the graphics device.
    auto vbConstantBufferPtr = m_vbConstantBuffer.get();
    context->VSSetConstantBuffers1(0, 1, &vbConstantBufferPtr, nullptr, nullptr);

    // Attach our pixel shader.
    context->PSSetShader(m_vbPixelShader.get(), nullptr, 0);

    // Set the texture in the shader
    auto samplerStatePtr = mVBSamplerState.get();
    context->PSSetSamplers(0, 1, &samplerStatePtr);
    auto textureViewPtr = mVBTextureView.get();
    context->PSSetShaderResources(0, 1, &textureViewPtr);

    // Draw the objects.
    context->DrawIndexed(m_vbMeshIndexCount, 0, 0);

    // Clear the shader resources, as the video background texture is now
    // the input for the next stage of rendering
    ID3D11ShaderResourceView* nullSRV[1] = { nullptr };
    context->PSSetShaderResources(0, 1, nullSRV);
}


void
DXRenderer::prepareForAugmentationRendering()
{
    auto context = mDeviceResources->GetD3DDeviceContext();

    context->RSSetState(mAugmentationRasterStateCullBack.get());
    context->OMSetDepthStencilState(mAugmentationDepthStencilState.get(), 1);
    context->OMSetBlendState(mAugmentationBlendState.get(), NULL, 0xffffffff);
}


void
DXRenderer::renderWorldOrigin(VuMatrix44F& projectionMatrix, VuMatrix44F& modelViewMatrix)
{
    DirectX::XMMATRIX projectionMatrixDX = convertVuforiaMatrixToDX(projectionMatrix);

    // Scale and convert the model-view to an XMMATRIX
    VuMatrix44F scaledModelViewMatrix;

    VuVector3F axisScaleVec{ WORLDORIGIN_AXES_SCALE, WORLDORIGIN_AXES_SCALE, WORLDORIGIN_AXES_SCALE };
    scaledModelViewMatrix = vuMatrix44FScale(axisScaleVec, modelViewMatrix);
    DirectX::XMMATRIX axisModelViewDX = convertVuforiaMatrixToDX(scaledModelViewMatrix);

    VuVector3F cubeScaleVec{ WORLDORIGIN_CUBE_SCALE, WORLDORIGIN_CUBE_SCALE, WORLDORIGIN_CUBE_SCALE };
    scaledModelViewMatrix = vuMatrix44FScale(cubeScaleVec, modelViewMatrix);
    DirectX::XMMATRIX cubeModelViewDX = convertVuforiaMatrixToDX(scaledModelViewMatrix);

    // Draw axes
    VuVector3F axis10cmSize{ 0.1f, 0.1f, 0.1f };
    renderAxis(projectionMatrix, modelViewMatrix, axis10cmSize);

    // Draw cube
    DirectX::XMVECTOR color = DirectX::Colors::LightGray;
    renderConstColor(projectionMatrixDX, cubeModelViewDX, color, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST, mCubeVertexBuffer,
                     mCubeSolidIndexBuffer, NUM_CUBE_INDEX);
}


void
DXRenderer::renderImageTarget(VuMatrix44F& projectionMatrix, VuMatrix44F& modelViewMatrix, VuMatrix44F& scaledModelViewMatrix)
{
    DirectX::XMMATRIX projectionMatrixDX = convertVuforiaMatrixToDX(projectionMatrix);

    // Draw translucent overlay and bounding box
    DirectX::XMMATRIX boundingBoxModelViewDX = convertVuforiaMatrixToDX(scaledModelViewMatrix);
    DirectX::XMVECTOR solidColor = DirectX::XMLoadFloat4(&SampleColors::RED_ALPHA10);
    renderConstColor(projectionMatrixDX, boundingBoxModelViewDX, solidColor, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST, mSquareVertexBuffer,
                     mSquareSolidIndexBuffer, NUM_SQUARE_INDEX);

    DirectX::XMVECTOR wireframeColor = DirectX::Colors::Red;
    renderConstColor(projectionMatrixDX, boundingBoxModelViewDX, wireframeColor, D3D11_PRIMITIVE_TOPOLOGY_LINELIST, mSquareVertexBuffer,
                     mSquareWireframeIndexBuffer, NUM_SQUARE_WIREFRAME_INDEX);

    // Draw astronaut
    DirectX::XMMATRIX astronautModelViewDX = convertVuforiaMatrixToDX(modelViewMatrix);
    renderModel(projectionMatrixDX, astronautModelViewDX, mAstronautVertexBuffer, mAstronautVertexCount, mAstronautTexture.get());

    // Draw axis
    VuVector3F axis2cmSize{ 0.02f, 0.02f, 0.02f };
    renderAxis(projectionMatrix, modelViewMatrix, axis2cmSize);

    mDeviceResources->GetD3DDeviceContext()->OMSetBlendState(nullptr, 0, 0xffffffff);
}


void
DXRenderer::renderModelTarget(VuMatrix44F& projectionMatrix, VuMatrix44F& modelViewMatrix, VuMatrix44F& /*scaledModelViewMatrix*/)
{
    DirectX::XMMATRIX projectionMatrixDX = convertVuforiaMatrixToDX(projectionMatrix);

    // Draw lander
    DirectX::XMMATRIX landerModelViewDX = convertVuforiaMatrixToDX(modelViewMatrix);
    renderModel(projectionMatrixDX, landerModelViewDX, mLanderVertexBuffer, mLanderVertexCount, mLanderTexture.get());

    // Draw axis
    VuVector3F axis10cmSize{ 0.1f, 0.1f, 0.1f };
    renderAxis(projectionMatrix, modelViewMatrix, axis10cmSize);

    mDeviceResources->GetD3DDeviceContext()->OMSetBlendState(nullptr, 0, 0xffffffff);
}


void
DXRenderer::renderModelTargetGuideView(VuMatrix44F& projectionMatrix, VuMatrix44F& modelViewMatrix, const VuImageInfo& image,
                                       VuBool guideViewImageHasChanged)
{
    DirectX::XMMATRIX projectionMatrixDX = convertVuforiaMatrixToDX(projectionMatrix);
    DirectX::XMMATRIX modelViewDX = convertVuforiaMatrixToDX(modelViewMatrix);

    // The guide view image is updated if the device orientation changes.
    // This is indicated by the guideViewImageHasChanged flag. In that case,
    // recreate the texture with the latest content of the image.
    if (mGuideViewTexture == nullptr || guideViewImageHasChanged == VU_TRUE)
    {
        mGuideViewTexture = std::make_unique<SampleCommon::Texture>(mDeviceResources);
        mGuideViewTexture->CreateFromVuforiaImage(image);
        mGuideViewTexture->Init();
    }

    auto context = mDeviceResources->GetD3DDeviceContext();

    SampleCommon::TexturedShaderConstantBuffer constantBufferData;
    // Set model-view matrix
    XMStoreFloat4x4(&constantBufferData.modelView, modelViewDX);
    // Set projection matrix
    XMStoreFloat4x4(&constantBufferData.projection, projectionMatrixDX);

    // Upload updated constant buffer.
    context->UpdateSubresource(mTexturedConstantBuffer.get(), 0, NULL, &constantBufferData, 0, 0);

    context->OMSetBlendState(mAugmentationBlendState.get(), NULL, 0xffffffff);
    context->IASetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    context->IASetInputLayout(mTexturedInputLayout.get());

    UINT stride = sizeof(SampleCommon::TexturedShaderInputBuffer);
    UINT offset = 0;
    auto guideViewVertexBufferPtr = mGuideViewVertexBuffer.get();
    context->IASetVertexBuffers(0, 1, &guideViewVertexBufferPtr, &stride, &offset);

    context->VSSetShader(mTexturedVertexShader.get(), nullptr, 0);
    auto texturedConstantBufferPtr = mTexturedConstantBuffer.get();
    context->VSSetConstantBuffers(0, 1, &texturedConstantBufferPtr);

    context->PSSetShader(mTexturedPixelShader.get(), nullptr, 0);
    // Set texture
    auto sampleStatePtr = mGuideViewTexture->GetD3DSamplerState().get();
    context->PSSetSamplers(0, 1, &sampleStatePtr);
    auto textureViewPtr = mGuideViewTexture->GetD3DTextureView().get();
    context->PSSetShaderResources(0, 1, &textureViewPtr);

    context->Draw(NUM_GUIDE_VIEW_VERTEX, 0);
}


// Private methods

void
DXRenderer::initConstColorVertexShader(const std::vector<byte>& fileData)
{
    winrt::check_hresult(
        mDeviceResources->GetD3DDevice()->CreateVertexShader(&fileData[0], fileData.size(), nullptr, mConstColorVertexShader.put()));

    CD3D11_BUFFER_DESC constantBufferDesc(sizeof(SampleCommon::ConstColorShaderConstantBuffer), D3D11_BIND_CONSTANT_BUFFER);

    winrt::check_hresult(mDeviceResources->GetD3DDevice()->CreateBuffer(&constantBufferDesc, nullptr, mConstColorConstantBuffer.put()));

    static const D3D11_INPUT_ELEMENT_DESC vertexDesc[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };

    winrt::check_hresult(mDeviceResources->GetD3DDevice()->CreateInputLayout(vertexDesc, ARRAYSIZE(vertexDesc), &fileData[0],
                                                                             fileData.size(), mConstColorInputLayout.put()));
}


void
DXRenderer::initConstColorPixelShader(const std::vector<byte>& fileData)
{
    winrt::check_hresult(
        mDeviceResources->GetD3DDevice()->CreatePixelShader(&fileData[0], fileData.size(), nullptr, mConstColorPixelShader.put()));
}


void
DXRenderer::initVertexColorVertexShader(const std::vector<byte>& fileData)
{
    LOG("initVertexColorVertexShader");

    winrt::check_hresult(
        mDeviceResources->GetD3DDevice()->CreateVertexShader(&fileData[0], fileData.size(), nullptr, mVertexColorVertexShader.put()));

    CD3D11_BUFFER_DESC constantBufferDesc(sizeof(SampleCommon::VertexColorShaderConstantBuffer), D3D11_BIND_CONSTANT_BUFFER);

    winrt::check_hresult(mDeviceResources->GetD3DDevice()->CreateBuffer(&constantBufferDesc, nullptr, mVertexColorConstantBuffer.put()));

    static const D3D11_INPUT_ELEMENT_DESC vertexDesc[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "COLOR", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };

    winrt::check_hresult(mDeviceResources->GetD3DDevice()->CreateInputLayout(vertexDesc, ARRAYSIZE(vertexDesc), &fileData[0],
                                                                             fileData.size(), mVertexColorInputLayout.put()));
}


void
DXRenderer::initVertexColorPixelShader(const std::vector<byte>& fileData)
{
    LOG("initVertexColorPixelShader");

    winrt::check_hresult(
        mDeviceResources->GetD3DDevice()->CreatePixelShader(&fileData[0], fileData.size(), nullptr, mVertexColorPixelShader.put()));
}


void
DXRenderer::initTexturedVertexShader(const std::vector<byte>& fileData)
{
    winrt::check_hresult(
        mDeviceResources->GetD3DDevice()->CreateVertexShader(&fileData[0], fileData.size(), nullptr, mTexturedVertexShader.put()));

    CD3D11_BUFFER_DESC constantBufferDesc(sizeof(SampleCommon::TexturedShaderConstantBuffer), D3D11_BIND_CONSTANT_BUFFER);

    winrt::check_hresult(mDeviceResources->GetD3DDevice()->CreateBuffer(&constantBufferDesc, nullptr, mTexturedConstantBuffer.put()));

    static const D3D11_INPUT_ELEMENT_DESC vertexDesc[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };

    winrt::check_hresult(mDeviceResources->GetD3DDevice()->CreateInputLayout(vertexDesc, ARRAYSIZE(vertexDesc), &fileData[0],
                                                                             fileData.size(), mTexturedInputLayout.put()));
}


void
DXRenderer::initTexturedPixelShader(const std::vector<byte>& fileData)
{
    winrt::check_hresult(
        mDeviceResources->GetD3DDevice()->CreatePixelShader(&fileData[0], fileData.size(), nullptr, mTexturedPixelShader.put()));
}


void
DXRenderer::initVideoBackgroundVertexShader(const std::vector<byte>& fileData)
{
    LOG("initVideoBackgroundVertexShader");

    winrt::check_hresult(
        mDeviceResources->GetD3DDevice()->CreateVertexShader(&fileData[0], fileData.size(), nullptr, m_vbVertexShader.put()));

    CD3D11_BUFFER_DESC constantBufferDesc(sizeof(SampleCommon::VideoBackgroundShaderConstantBuffer), D3D11_BIND_CONSTANT_BUFFER);

    winrt::check_hresult(mDeviceResources->GetD3DDevice()->CreateBuffer(&constantBufferDesc, nullptr, m_vbConstantBuffer.put()));

    static const D3D11_INPUT_ELEMENT_DESC vertexDesc[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };

    winrt::check_hresult(mDeviceResources->GetD3DDevice()->CreateInputLayout(vertexDesc, ARRAYSIZE(vertexDesc), &fileData[0],
                                                                             fileData.size(), m_vbInputLayout.put()));
}


void
DXRenderer::initVideoBackgroundPixelShader(const std::vector<byte>& fileData)
{
    LOG("initVideoBackgroundPixelShader");

    winrt::check_hresult(
        mDeviceResources->GetD3DDevice()->CreatePixelShader(&fileData[0], fileData.size(), nullptr, m_vbPixelShader.put()));
}


void
DXRenderer::initVideoBackgroundRenderState()
{
    LOG("initVideoBackgroundRenderState");

    // setup the rasterizer
    auto context = mDeviceResources->GetD3DDeviceContext();

    ID3D11Device* device;
    context->GetDevice(&device);

    // Create the rasterizers for video background rendering
    D3D11_RASTERIZER_DESC videoRasterDescCounterClockwise = DX::CreateRasterizerDesc(D3D11_FILL_SOLID, // rendering
                                                                                     D3D11_CULL_NONE,  // culling
                                                                                     true,             // Counter Clockwise front face
                                                                                     false             // no depth clipping
    );
    device->CreateRasterizerState(&videoRasterDescCounterClockwise, mVBRasterStateCounterClockwise.put());

    // Create Depth-Stencil State without depth testing
    // for video background rendering
    D3D11_DEPTH_STENCIL_DESC videoDepthStencilDesc =
        DX::CreateDepthStencilDesc(false, D3D11_DEPTH_WRITE_MASK_ZERO, D3D11_COMPARISON_ALWAYS);
    device->CreateDepthStencilState(&videoDepthStencilDesc, mVBDepthStencilState.put());

    // Create blend state for video background rendering
    D3D11_BLEND_DESC videoBackgroundBlendDesc = DX::CreateBlendDesc(false);
    device->CreateBlendState(&videoBackgroundBlendDesc, mVBBlendState.put());
}


void
DXRenderer::initVideoBackgroundMesh(const int numVertices, const float* vertices, const float* texCoords, const int numTriangles,
                                    const unsigned int* vbIndices, ID3D11Device* device)
{
    LOG("initVideoBackgroundMesh");

    // Setup the vertex and index buffers
    m_vbMeshIndexCount = numTriangles * 3;

    SampleCommon::VideoBackgroundShaderInputBuffer* vbMeshVertices = new SampleCommon::VideoBackgroundShaderInputBuffer[numVertices];

    for (int i = 0; i < numVertices; ++i)
    {
        vbMeshVertices[i].pos = DirectX::XMFLOAT3(vertices[(i * 3)], vertices[(i * 3) + 1], vertices[(i * 3) + 2]);

        vbMeshVertices[i].texcoord = DirectX::XMFLOAT2(texCoords[(i * 2)], texCoords[(i * 2) + 1]);
    }

    D3D11_SUBRESOURCE_DATA vertexBufferData = { 0 };
    vertexBufferData.pSysMem = vbMeshVertices;
    vertexBufferData.SysMemPitch = 0;
    vertexBufferData.SysMemSlicePitch = 0;
    CD3D11_BUFFER_DESC vertexBufferDesc(numVertices * sizeof(SampleCommon::VideoBackgroundShaderInputBuffer), D3D11_BIND_VERTEX_BUFFER);
    winrt::check_hresult(device->CreateBuffer(&vertexBufferDesc, &vertexBufferData, m_vbVertexBuffer.put()));
    delete[] vbMeshVertices;
    vbMeshVertices = nullptr;

    D3D11_SUBRESOURCE_DATA indexBufferData = { 0 };
    indexBufferData.pSysMem = vbIndices;
    indexBufferData.SysMemPitch = 0;
    indexBufferData.SysMemSlicePitch = 0;

    CD3D11_BUFFER_DESC indexBufferDesc(m_vbMeshIndexCount * sizeof(unsigned int), D3D11_BIND_INDEX_BUFFER);

    winrt::check_hresult(device->CreateBuffer(&indexBufferDesc, &indexBufferData, mVBIndexBuffer.put()));

    mVideoBackgroundMeshInitialized = true;
}


void
DXRenderer::initSquare()
{
    static SampleCommon::ConstColorShaderInputBuffer squareVerticesDX[NUM_SQUARE_VERTEX];
    for (int i = 0, j = 0; i < NUM_SQUARE_VERTEX; ++i)
    {
        squareVerticesDX[i].pos.x = squareVertices[j++];
        squareVerticesDX[i].pos.y = squareVertices[j++];
        squareVerticesDX[i].pos.z = squareVertices[j++];
    }

    D3D11_SUBRESOURCE_DATA vertexBufferData = { 0 };
    vertexBufferData.pSysMem = squareVerticesDX;
    vertexBufferData.SysMemPitch = 0;
    vertexBufferData.SysMemSlicePitch = 0;
    CD3D11_BUFFER_DESC vertexBufferDesc(sizeof(squareVerticesDX), D3D11_BIND_VERTEX_BUFFER);
    winrt::check_hresult(mDeviceResources->GetD3DDevice()->CreateBuffer(&vertexBufferDesc, &vertexBufferData, mSquareVertexBuffer.put()));


    D3D11_SUBRESOURCE_DATA wireframeIndexBufferData = { 0 };
    wireframeIndexBufferData.pSysMem = squareWireframeIndices;
    wireframeIndexBufferData.SysMemPitch = 0;
    wireframeIndexBufferData.SysMemSlicePitch = 0;
    CD3D11_BUFFER_DESC wireframeIndexBufferDesc(sizeof(squareWireframeIndices), D3D11_BIND_INDEX_BUFFER);
    winrt::check_hresult(mDeviceResources->GetD3DDevice()->CreateBuffer(&wireframeIndexBufferDesc, &wireframeIndexBufferData,
                                                                        mSquareWireframeIndexBuffer.put()));


    D3D11_SUBRESOURCE_DATA solidIndexBufferData = { 0 };
    solidIndexBufferData.pSysMem = squareIndices;
    solidIndexBufferData.SysMemPitch = 0;
    solidIndexBufferData.SysMemSlicePitch = 0;
    CD3D11_BUFFER_DESC solidIndexBufferDesc(sizeof(squareIndices), D3D11_BIND_INDEX_BUFFER);
    winrt::check_hresult(
        mDeviceResources->GetD3DDevice()->CreateBuffer(&solidIndexBufferDesc, &solidIndexBufferData, mSquareSolidIndexBuffer.put()));
}


void
DXRenderer::initCube()
{
    static SampleCommon::ConstColorShaderInputBuffer cubeVerticesDX[NUM_CUBE_VERTEX];
    for (int i = 0, j = 0; i < NUM_CUBE_VERTEX; ++i)
    {
        cubeVerticesDX[i].pos.x = cubeVertices[j++];
        cubeVerticesDX[i].pos.y = cubeVertices[j++];
        cubeVerticesDX[i].pos.z = cubeVertices[j++];
    }

    D3D11_SUBRESOURCE_DATA vertexBufferData = { 0 };
    vertexBufferData.pSysMem = cubeVerticesDX;
    vertexBufferData.SysMemPitch = 0;
    vertexBufferData.SysMemSlicePitch = 0;
    CD3D11_BUFFER_DESC vertexBufferDesc(sizeof(cubeVerticesDX), D3D11_BIND_VERTEX_BUFFER);
    winrt::check_hresult(mDeviceResources->GetD3DDevice()->CreateBuffer(&vertexBufferDesc, &vertexBufferData, mCubeVertexBuffer.put()));


    D3D11_SUBRESOURCE_DATA wireframeIndexBufferData = { 0 };
    wireframeIndexBufferData.pSysMem = cubeWireframeIndices;
    wireframeIndexBufferData.SysMemPitch = 0;
    wireframeIndexBufferData.SysMemSlicePitch = 0;
    CD3D11_BUFFER_DESC wireframeIndexBufferDesc(sizeof(cubeWireframeIndices), D3D11_BIND_INDEX_BUFFER);
    winrt::check_hresult(mDeviceResources->GetD3DDevice()->CreateBuffer(&wireframeIndexBufferDesc, &wireframeIndexBufferData,
                                                                        mCubeWireframeIndexBuffer.put()));

    D3D11_SUBRESOURCE_DATA solidIndexBufferData = { 0 };
    solidIndexBufferData.pSysMem = cubeIndices;
    solidIndexBufferData.SysMemPitch = 0;
    solidIndexBufferData.SysMemSlicePitch = 0;
    CD3D11_BUFFER_DESC solidIndexBufferDesc(sizeof(cubeIndices), D3D11_BIND_INDEX_BUFFER);
    winrt::check_hresult(
        mDeviceResources->GetD3DDevice()->CreateBuffer(&solidIndexBufferDesc, &solidIndexBufferData, mCubeSolidIndexBuffer.put()));
}


void
DXRenderer::initAxis()
{
    LOG("initAxis");

    static SampleCommon::VertexColorShaderInputBuffer axisVerticesDX[NUM_AXIS_VERTEX];
    for (int i = 0, j = 0, k = 0; i < NUM_AXIS_VERTEX; ++i)
    {
        axisVerticesDX[i].pos.x = axisVertices[j++];
        axisVerticesDX[i].pos.y = axisVertices[j++];
        axisVerticesDX[i].pos.z = axisVertices[j++];
        axisVerticesDX[i].color.x = axisColors[k++];
        axisVerticesDX[i].color.y = axisColors[k++];
        axisVerticesDX[i].color.z = axisColors[k++];
        k++; // skip alpha
    }

    D3D11_SUBRESOURCE_DATA vertexBufferData = { 0 };
    vertexBufferData.pSysMem = axisVerticesDX;
    vertexBufferData.SysMemPitch = 0;
    vertexBufferData.SysMemSlicePitch = 0;
    CD3D11_BUFFER_DESC vertexBufferDesc(sizeof(axisVerticesDX), D3D11_BIND_VERTEX_BUFFER);
    winrt::check_hresult(mDeviceResources->GetD3DDevice()->CreateBuffer(&vertexBufferDesc, &vertexBufferData, mAxesVertexBuffer.put()));

    D3D11_SUBRESOURCE_DATA indexBufferData = { 0 };
    indexBufferData.pSysMem = axisIndices;
    indexBufferData.SysMemPitch = 0;
    indexBufferData.SysMemSlicePitch = 0;
    CD3D11_BUFFER_DESC indexBufferDesc(sizeof(axisIndices), D3D11_BIND_INDEX_BUFFER);
    winrt::check_hresult(mDeviceResources->GetD3DDevice()->CreateBuffer(&indexBufferDesc, &indexBufferData, mAxesIndexBuffer.put()));
}


void
DXRenderer::initGuideView()
{
    // Initialize vertex buffer.
    SampleCommon::TexturedShaderInputBuffer topLeft{ {
                                                         -0.5f,
                                                         0.5f,
                                                         0.0f,
                                                     },
                                                     { 0.0f, 0.0f } };

    SampleCommon::TexturedShaderInputBuffer topRight{ {
                                                          0.5f,
                                                          0.5f,
                                                          0.0f,
                                                      },
                                                      { 1.0f, 0.0f } };

    SampleCommon::TexturedShaderInputBuffer bottomLeft{ {
                                                            -0.5f,
                                                            -0.5f,
                                                            0.0f,
                                                        },
                                                        { 0.0f, 1.0f } };

    SampleCommon::TexturedShaderInputBuffer bottomRight{ {
                                                             0.5f,
                                                             -0.5f,
                                                             0.0f,
                                                         },
                                                         { 1.0f, 1.0f } };

    SampleCommon::TexturedShaderInputBuffer meshVertices[NUM_GUIDE_VIEW_VERTEX] = { topLeft, bottomLeft,  bottomRight,
                                                                                    topLeft, bottomRight, topRight };

    D3D11_SUBRESOURCE_DATA vertexBufferData = { 0 };
    vertexBufferData.pSysMem = meshVertices;
    vertexBufferData.SysMemPitch = 0;
    vertexBufferData.SysMemSlicePitch = 0;
    CD3D11_BUFFER_DESC vertexBufferDesc(sizeof(meshVertices), D3D11_BIND_VERTEX_BUFFER);
    winrt::check_hresult(
        mDeviceResources->GetD3DDevice()->CreateBuffer(&vertexBufferDesc, &vertexBufferData, mGuideViewVertexBuffer.put()));
}


void
DXRenderer::initModels()
{
    LOG("initModels");

    auto loadAstronautModel = DX::ReadDataAsync(RES_PATH_ASTRONAUT_MODEL).then([this](const std::vector<byte>& fileData) {
        MemoryInputStream fileDataStream(reinterpret_cast<const char*>(fileData.data()), fileData.size());

        tinyobj::attrib_t attrib;
        std::vector<tinyobj::shape_t> shapes;
        std::vector<tinyobj::material_t> materials;

        std::string warn;
        std::string err;

        bool ret = tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, &fileDataStream);

        if (!ret || !err.empty())
        {
            LOG("Error loading Astronaut model (%s)", err.c_str());
            throw winrt::hresult_error(E_FAIL, winrt::to_hstring("Error loading Astronaut obj model"));
        }
        if (!warn.empty())
        {
            LOG("Warning when loading Astronaut model (%s)", warn.c_str());
        }

        mAstronautVertexCount = initBuffersFromModel(attrib, shapes, materials, mAstronautVertexBuffer.put());

        mAstronautTexture = std::make_unique<SampleCommon::Texture>(mDeviceResources);
        mAstronautTexture->CreateFromFile(RES_PATH_ASTRONAUT_TEXTURE.c_str());
        mAstronautTexture->Init();
    });

    auto loadLanderModel = DX::ReadDataAsync(RES_PATH_LANDER_MODEL).then([this](const std::vector<byte>& fileData) {
        MemoryInputStream fileDataStream(reinterpret_cast<const char*>(fileData.data()), fileData.size());

        tinyobj::attrib_t attrib;
        std::vector<tinyobj::shape_t> shapes;
        std::vector<tinyobj::material_t> materials;

        std::string warn;
        std::string err;

        bool ret = tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, &fileDataStream);

        if (!ret || !err.empty())
        {
            LOG("Error loading Lander model (%s)", err.c_str());
            throw winrt::hresult_error(E_FAIL, winrt::to_hstring("Error loading Lander obj model"));
        }
        if (!warn.empty())
        {
            LOG("Warning when loading Lander model (%s)", warn.c_str());
        }

        mLanderVertexCount = initBuffersFromModel(attrib, shapes, materials, mLanderVertexBuffer.put());

        mLanderTexture = std::make_unique<SampleCommon::Texture>(mDeviceResources);
        mLanderTexture->CreateFromFile(RES_PATH_LANDER_TEXTURE.c_str());
        mLanderTexture->Init();
    });

    loadAstronautModel.get();
    loadLanderModel.get();
}


int
DXRenderer::initBuffersFromModel(tinyobj::attrib_t& attrib, std::vector<tinyobj::shape_t>& shapes,
                                 std::vector<tinyobj::material_t>& /*materials*/, ID3D11Buffer** vertexBuffer)
{
    int numVertices = 0;

    // Get the count of vertices
    for (size_t s = 0; s < shapes.size(); s++)
    {
        for (size_t f = 0; f < shapes[s].mesh.num_face_vertices.size(); f++)
        {
            numVertices += shapes[s].mesh.num_face_vertices[f];
        }
    }

    SampleCommon::TexturedShaderInputBuffer* inputBufferRaw = new SampleCommon::TexturedShaderInputBuffer[numVertices];

    // The winding order of vertices in the obj files is OpenGL style counter-clockwise
    // We could convert that to the DX convention, instead we have set
    // the RasterizerState for counter-clockwise.

    // Loop over shapes
    // s is the index into the shapes vector
    // f is the index of the current face
    // v is the index of the current vertex
    // i is the index into the target buffer
    for (size_t s = 0, i = 0; s < shapes.size(); ++s)
    {
        // Loop over faces(polygon)
        size_t index_offset = 0;
        for (size_t f = 0; f < shapes[s].mesh.num_face_vertices.size(); ++f)
        {
            int fv = shapes[s].mesh.num_face_vertices[f];

            // Loop over vertices in the face.
            for (size_t v = 0; v < size_t(fv); ++v, ++i)
            {
                // access to vertex
                tinyobj::index_t idx = shapes[s].mesh.indices[index_offset + v];

                inputBufferRaw[i].pos =
                    DirectX::XMFLOAT3(attrib.vertices[3L * idx.vertex_index + 0], attrib.vertices[3L * idx.vertex_index + 1],
                                      attrib.vertices[3L * idx.vertex_index + 2]);

                // The model may not have texture coordinates for every vertex
                // If a texture coordinate is missing we just set it to 0,0
                // This may not be suitable for rendering some OBJ model files
                if (idx.texcoord_index < 0)
                {
                    inputBufferRaw[i].texcoord = DirectX::XMFLOAT2(0.f, 0.f);
                }
                else
                {
                    inputBufferRaw[i].texcoord =
                        DirectX::XMFLOAT2(attrib.texcoords[2L * idx.texcoord_index + 0],
                                          1.0f - attrib.texcoords[2L * idx.texcoord_index + 1] // vertical flip, convert GL to DX
                        );
                }
            }
            index_offset += fv;
        }
    }

    D3D11_SUBRESOURCE_DATA vertexBufferData = { 0 };
    vertexBufferData.pSysMem = inputBufferRaw;
    vertexBufferData.SysMemPitch = 0;
    vertexBufferData.SysMemSlicePitch = 0;
    CD3D11_BUFFER_DESC vertexBufferDesc(numVertices * sizeof(SampleCommon::TexturedShaderInputBuffer), D3D11_BIND_VERTEX_BUFFER);
    winrt::check_hresult(mDeviceResources->GetD3DDevice()->CreateBuffer(&vertexBufferDesc, &vertexBufferData, vertexBuffer));
    delete[] inputBufferRaw;
    inputBufferRaw = nullptr;

    return numVertices;
}


DirectX::XMMATRIX
DXRenderer::convertVuforiaMatrixToDX(const VuMatrix44F& vuforiaMatrix)
{
    DirectX::XMFLOAT4X4 projectFloat4x4;
    memcpy(projectFloat4x4.m, vuforiaMatrix.data, sizeof(float) * 16);
    XMStoreFloat4x4(&projectFloat4x4, DirectX::XMMatrixTranspose(XMLoadFloat4x4(&projectFloat4x4)));
    return XMLoadFloat4x4(&projectFloat4x4);
}


void
DXRenderer::renderConstColor(const DirectX::XMMATRIX& projection, const DirectX::XMMATRIX& modelView, DirectX::XMVECTOR& color,
                             D3D_PRIMITIVE_TOPOLOGY topology, winrt::com_ptr<ID3D11Buffer> verticies, winrt::com_ptr<ID3D11Buffer> indicies,
                             uint32_t indexCount)
{
    auto context = mDeviceResources->GetD3DDeviceContext();

    SampleCommon::ConstColorShaderConstantBuffer constantBufferData;

    // Set model-view matrix
    XMStoreFloat4x4(&constantBufferData.modelView, modelView);
    // Set projection matrix
    XMStoreFloat4x4(&constantBufferData.projection, projection);
    // set color
    XMStoreFloat4(&constantBufferData.color, color);

    // Prepare the constant buffer to send it to the graphics device.
    context->UpdateSubresource1(mConstColorConstantBuffer.get(), 0, NULL, &constantBufferData, 0, 0, 0);

    // Each vertex is one instance of the WireframeVertexPosition struct.
    UINT stride = sizeof(SampleCommon::ConstColorShaderInputBuffer);
    UINT offset = 0;
    auto verticiesPtr = verticies.get();
    context->IASetVertexBuffers(0, 1, &verticiesPtr, &stride, &offset);

    context->IASetIndexBuffer(indicies.get(),
                              DXGI_FORMAT_R16_UINT, // Each index is one 16-bit unsigned integer (short).
                              0);

    context->IASetPrimitiveTopology(topology);

    context->IASetInputLayout(mConstColorInputLayout.get());

    // Attach our vertex shader.
    context->VSSetShader(mConstColorVertexShader.get(), nullptr, 0);

    // Send the constant buffer to the graphics device.
    auto constColorConstantBufferPtr = mConstColorConstantBuffer.get();
    context->VSSetConstantBuffers1(0, 1, &constColorConstantBufferPtr, nullptr, nullptr);

    // Attach our pixel shader.
    context->PSSetShader(mConstColorPixelShader.get(), nullptr, 0);

    // Draw the objects.
    context->DrawIndexed(indexCount, 0, 0);
}


void
DXRenderer::renderAxis(const VuMatrix44F& projectionMatrix, const VuMatrix44F& modelViewMatrix, const VuVector3F& scale)
{
    auto context = mDeviceResources->GetD3DDeviceContext();

    SampleCommon::VertexColorShaderConstantBuffer constantBufferData;

    // Set model-view matrix
    auto adjustedModelView = vuMatrix44FScale(scale, modelViewMatrix);
    auto modelView = convertVuforiaMatrixToDX(adjustedModelView);
    XMStoreFloat4x4(&constantBufferData.modelView, modelView);
    // Set projection matrix
    auto projection = convertVuforiaMatrixToDX(projectionMatrix);
    XMStoreFloat4x4(&constantBufferData.projection, projection);

    // Prepare the constant buffer to send it to the graphics device.
    context->UpdateSubresource1(mVertexColorConstantBuffer.get(), 0, NULL, &constantBufferData, 0, 0, 0);

    // Each vertex is one instance of the VertexColorShaderInputBuffer struct.
    auto verticiesPtr = mAxesVertexBuffer.get();
    UINT stride = sizeof(SampleCommon::VertexColorShaderInputBuffer);
    UINT offset = 0;
    context->IASetVertexBuffers(0, 1, &verticiesPtr, &stride, &offset);

    context->IASetIndexBuffer(mAxesIndexBuffer.get(),
                              DXGI_FORMAT_R16_UINT, // Each index is one 16-bit unsigned integer (short).
                              0);

    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);

    context->IASetInputLayout(mVertexColorInputLayout.get());

    // Attach our vertex shader.
    context->VSSetShader(mVertexColorVertexShader.get(), nullptr, 0);

    // Send the constant buffer to the graphics device.
    auto vertexColorConstantBufferPtr = mVertexColorConstantBuffer.get();
    context->VSSetConstantBuffers1(0, 1, &vertexColorConstantBufferPtr, nullptr, nullptr);

    // Attach our pixel shader.
    context->PSSetShader(mVertexColorPixelShader.get(), nullptr, 0);

    // Draw the objects.
    context->DrawIndexed(NUM_AXIS_INDEX, 0, 0);
}


void
DXRenderer::renderModel(const DirectX::XMMATRIX& projection, const DirectX::XMMATRIX& modelView, winrt::com_ptr<ID3D11Buffer> vertices,
                        int numVertices, SampleCommon::Texture* texture)
{
    assert(texture != nullptr);
    auto context = mDeviceResources->GetD3DDeviceContext();

    // Set the projection matrix
    SampleCommon::TexturedShaderConstantBuffer vbConstantBufferData;
    XMStoreFloat4x4(&vbConstantBufferData.modelView, modelView);
    XMStoreFloat4x4(&vbConstantBufferData.projection, projection);
    // Prepare the constant buffer to send it to the graphics device.
    context->UpdateSubresource1(mTexturedConstantBuffer.get(), 0, NULL, &vbConstantBufferData, 0, 0, 0);

    // Each vertex is one instance of the input buffer struct.
    UINT stride = sizeof(SampleCommon::TexturedShaderInputBuffer);
    UINT offset = 0;
    auto vertexBufferPtr = vertices.get();
    context->IASetVertexBuffers(0, 1, &vertexBufferPtr, &stride, &offset);

    context->IASetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    context->IASetInputLayout(mTexturedInputLayout.get());

    // Attach our vertex shader.
    context->VSSetShader(mTexturedVertexShader.get(), nullptr, 0);

    // Send the constant buffer to the graphics device.
    auto texturedConstantBufferPtr = mTexturedConstantBuffer.get();
    context->VSSetConstantBuffers1(0, 1, &texturedConstantBufferPtr, nullptr, nullptr);

    // Attach our pixel shader.
    context->PSSetShader(mTexturedPixelShader.get(), nullptr, 0);

    // Set the texture in the shader
    auto samplerStatePtr = texture->GetD3DSamplerState().get();
    context->PSSetSamplers(0, 1, &samplerStatePtr);
    auto textureViewPtr = texture->GetD3DTextureView().get();
    context->PSSetShaderResources(0, 1, &textureViewPtr);

    // Draw the objects.
    context->Draw(numVertices, 0);

    // Clear the shader resources, as the texture is now
    // the input for the next stage of rendering
    ID3D11ShaderResourceView* nullSRV[1] = { nullptr };
    context->PSSetShaderResources(0, 1, nullSRV);
}

} // namespace winrt::VuforiaSample::implementation
