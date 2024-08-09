/*===============================================================================
Copyright (c) 2023 PTC Inc. and/or Its Subsidiary Companies. All Rights Reserved.

Vuforia is a trademark of PTC Inc., registered in the United States and other
countries.
===============================================================================*/

#pragma once

#include "DeviceResources.h"
#include "ShaderStructures.h"
#include <atomic>
#include <memory>

#include <tiny_obj_loader.h>

#include <VuforiaEngine/VuforiaEngine.h>


namespace SampleCommon
{
class Texture; // forward reference
}

namespace winrt::VuforiaSample::implementation
{
/// Class to encapsulate DirectX rendering for the sample
class DXRenderer
{
public:
    DXRenderer(const std::shared_ptr<winrt::DX::DeviceResources>& deviceResources);
    ~DXRenderer();

    void CreateDeviceDependentResources();
    void CreateWindowSizeDependentResources();
    void ReleaseDeviceDependentResources();

    bool isRendererInitialized() { return mRendererInitialized; }

    bool isVideoBackgroundTextureInitialized() { return mVideoBackgroundTextureInitialized; }
    void initVideoBackgroundTexture(size_t width, size_t height);
    ID3D11Texture2D* getVideoBackgroundTexture() { return mVBTexture.get(); }

    /// Render the video background
    void renderVideoBackground(const VuMatrix44F& projectionMatrix, const int numVertices, const float* vertices, const float* texCoords,
                               const int numTriangles, const unsigned int* indices);

    /// Must be called before the methods below that render augmentations to setup the pipeline
    void prepareForAugmentationRendering();

    /// Render augmentation for the world origin
    void renderWorldOrigin(VuMatrix44F& projectionMatrix, VuMatrix44F& modelViewMatrix);

    /// Render a bounding box augmentation on an Image Target
    void renderImageTarget(VuMatrix44F& projectionMatrix, VuMatrix44F& modelViewMatrix, VuMatrix44F& scaledModelViewMatrix);

    /// Render a bounding cube augmentation on a Model Target
    void renderModelTarget(VuMatrix44F& projectionMatrix, VuMatrix44F& modelViewMatrix, VuMatrix44F& scaledModelViewMatrix);

    /// Render the Guide View for a model target
    void renderModelTargetGuideView(VuMatrix44F& projectionMatrix, VuMatrix44F& modelViewMatrix, const VuImageInfo& image,
                                    VuBool guideViewImageHasChanged);

private: // methods
    void initConstColorVertexShader(const std::vector<byte>& fileData);
    void initConstColorPixelShader(const std::vector<byte>& fileData);
    void initVertexColorVertexShader(const std::vector<byte>& fileData);
    void initVertexColorPixelShader(const std::vector<byte>& fileData);
    void initTexturedVertexShader(const std::vector<byte>& fileData);
    void initTexturedPixelShader(const std::vector<byte>& fileData);
    void initVideoBackgroundVertexShader(const std::vector<byte>& fileData);
    void initVideoBackgroundPixelShader(const std::vector<byte>& fileData);
    void initVideoBackgroundRenderState();
    void initVideoBackgroundMesh(const int numVertices, const float* vertices, const float* texCoords, const int numTriangles,
                                 const unsigned int* indices, ID3D11Device* device);

    void initSquare();
    void initCube();
    void initAxis();
    void initGuideView();
    void initModels();
    int initBuffersFromModel(tinyobj::attrib_t& attrib, std::vector<tinyobj::shape_t>& shapes, std::vector<tinyobj::material_t>& materials,
                             ID3D11Buffer** vertexBuffer);

    DirectX::XMMATRIX convertVuforiaMatrixToDX(const VuMatrix44F& vuforiaMatrix);

    /// DirectX rendering utility to render an object with a single color
    /// Projection and Model-View are already converted to DirectX matrices
    void renderConstColor(const DirectX::XMMATRIX& projection, const DirectX::XMMATRIX& modelView, DirectX::XMVECTOR& color,
                          D3D_PRIMITIVE_TOPOLOGY topology, winrt::com_ptr<ID3D11Buffer> verticies, winrt::com_ptr<ID3D11Buffer> indicies,
                          uint32_t indexCount);

    /// DirectX rendering utility to render a set of axes
    /// Projection and Model-View are already converted to DirectX matrices
    void renderAxis(const VuMatrix44F& projectionMatrix, const VuMatrix44F& modelViewMatrix, const VuVector3F& scale);

    /// DirectX rendering utility to render a textured object
    /// Projection and Model-View are already converted to DirectX matrices
    void renderModel(const DirectX::XMMATRIX& projection, const DirectX::XMMATRIX& modelView, winrt::com_ptr<ID3D11Buffer> vertices,
                     int numVertices, SampleCommon::Texture* texture);

private: // data members
    // Cached pointer to device resources.
    std::shared_ptr<DX::DeviceResources> mDeviceResources;

    std::atomic<bool> mRendererInitialized;

    //
    // Video Background rendering
    //
    bool mVideoBackgroundMeshInitialized;
    bool mVideoBackgroundTextureInitialized;

    winrt::com_ptr<ID3D11RasterizerState> mVBRasterStateCounterClockwise;
    winrt::com_ptr<ID3D11DepthStencilState> mVBDepthStencilState;
    winrt::com_ptr<ID3D11BlendState> mVBBlendState;

    winrt::com_ptr<ID3D11InputLayout> m_vbInputLayout;
    winrt::com_ptr<ID3D11VertexShader> m_vbVertexShader;
    winrt::com_ptr<ID3D11PixelShader> m_vbPixelShader;
    winrt::com_ptr<ID3D11Buffer> m_vbConstantBuffer;
    // Vertices
    winrt::com_ptr<ID3D11Buffer> m_vbVertexBuffer;
    winrt::com_ptr<ID3D11Buffer> mVBIndexBuffer;
    int m_vbMeshIndexCount;

    // Texture
    winrt::com_ptr<ID3D11Texture2D> mVBTexture;
    winrt::com_ptr<ID3D11SamplerState> mVBSamplerState;
    winrt::com_ptr<ID3D11ShaderResourceView> mVBTextureView;
    size_t m_imageWidth;
    size_t m_imageHeight;


    //
    // Augmentation rendering
    //
    // DX States for augmentation rendering
    winrt::com_ptr<ID3D11RasterizerState> mAugmentationRasterStateCullBack;
    winrt::com_ptr<ID3D11DepthStencilState> mAugmentationDepthStencilState;
    winrt::com_ptr<ID3D11BlendState> mAugmentationBlendState;

    // Direct3D resources for the Const color shader
    winrt::com_ptr<ID3D11InputLayout> mConstColorInputLayout;
    winrt::com_ptr<ID3D11VertexShader> mConstColorVertexShader;
    winrt::com_ptr<ID3D11PixelShader> mConstColorPixelShader;
    winrt::com_ptr<ID3D11Buffer> mConstColorConstantBuffer;

    // Direct3D resources for the Vertex color shader
    winrt::com_ptr<ID3D11InputLayout> mVertexColorInputLayout;
    winrt::com_ptr<ID3D11VertexShader> mVertexColorVertexShader;
    winrt::com_ptr<ID3D11PixelShader> mVertexColorPixelShader;
    winrt::com_ptr<ID3D11Buffer> mVertexColorConstantBuffer;

    // Direct3D resources for the Textured vertex shader
    winrt::com_ptr<ID3D11InputLayout> mTexturedInputLayout;
    winrt::com_ptr<ID3D11VertexShader> mTexturedVertexShader;
    winrt::com_ptr<ID3D11PixelShader> mTexturedPixelShader;
    winrt::com_ptr<ID3D11Buffer> mTexturedConstantBuffer;

    // Vertices and Indices for drawing a 2D square
    winrt::com_ptr<ID3D11Buffer> mSquareVertexBuffer;
    winrt::com_ptr<ID3D11Buffer> mSquareSolidIndexBuffer;
    winrt::com_ptr<ID3D11Buffer> mSquareWireframeIndexBuffer;

    // Vertices and Indices for drawing a cube
    winrt::com_ptr<ID3D11Buffer> mCubeVertexBuffer;
    winrt::com_ptr<ID3D11Buffer> mCubeSolidIndexBuffer;
    winrt::com_ptr<ID3D11Buffer> mCubeWireframeIndexBuffer;

    // Vertices and Indices for drawing the axes
    winrt::com_ptr<ID3D11Buffer> mAxesVertexBuffer;
    winrt::com_ptr<ID3D11Buffer> mAxesIndexBuffer;

    winrt::com_ptr<ID3D11Buffer> mGuideViewVertexBuffer;
    std::unique_ptr<SampleCommon::Texture> mGuideViewTexture;

    // Data for rendering the astronaut
    winrt::com_ptr<ID3D11Buffer> mAstronautVertexBuffer;
    int mAstronautVertexCount;
    std::unique_ptr<SampleCommon::Texture> mAstronautTexture;

    // Data for rendering the lander
    winrt::com_ptr<ID3D11Buffer> mLanderVertexBuffer;
    int mLanderVertexCount;
    std::unique_ptr<SampleCommon::Texture> mLanderTexture;
};
} // namespace winrt::VuforiaSample::implementation
