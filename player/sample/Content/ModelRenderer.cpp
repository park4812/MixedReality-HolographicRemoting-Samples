//*********************************************************
//
// Copyright (c) Microsoft. All rights reserved.
// This code is licensed under the MIT License (MIT).
// THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
// IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
// PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
//
//*********************************************************

#include "pch.h"
#include "ModelRenderer.h"
#include "Common\DirectXHelper.h"
#include <mutex>
#include <string>

using namespace BasicHologram;
using namespace DirectX;
using namespace winrt::Windows::Foundation::Numerics;
using namespace winrt::Windows::UI::Input::Spatial;

// Loads vertex and pixel shaders from files and instantiates the cube geometry.
// "버텍스 셰이더와 픽셀 셰이더를 파일에서 불러와서 큐브 지오메트리를 인스턴스화합니다."
ModelRenderer::ModelRenderer(std::shared_ptr<DX::DeviceResources> const& deviceResources) :
    m_deviceResources(deviceResources)
{
    CreateDeviceDependentResources();

    XMStoreFloat4x4(&m_modelTransform, DirectX::XMMatrixIdentity());
    XMStoreFloat4x4(&m_groupTransform, DirectX::XMMatrixIdentity());

    m_radiansY = static_cast<float>(XM_PI / 2);
    m_fGroupScale = 1.0f;
}

// This function uses a SpatialPointerPose to position the world-locked hologram
// two meters in front of the user's heading.
// 이 함수는 SpatialPointerPose를 사용하여 사용자의 시선 방향으로부터 두 미터 앞에 월드-락 홀로그램을 위치시킵니다.
void ModelRenderer::PositionHologram(SpatialPointerPose const& pointerPose, const DX::StepTimer& timer)
{
    const float& deltaTime = static_cast<float>(timer.GetElapsedSeconds());

    if (pointerPose != nullptr)
    {
        // Get the gaze direction relative to the given coordinate system.
        // 주어진 좌표계에 대해 시선 방향을 가져옵니다.
        const float3 headPosition = pointerPose.Head().Position();
        const float3 headDirection = pointerPose.Head().ForwardDirection();

        constexpr float distanceFromUser = 2.0f; // meters
        const float3 gazeAtTwoMeters = headPosition + (distanceFromUser * headDirection);

        // Use linear interpolation to smooth the position over time. This keeps the hologram 
        // comfortably stable.
        // 시간에 따라 위치를 선형 보간을 사용하여 부드럽게 합니다. 이것은 홀로그램을 편안하게 안정적으로 유지합니다.
        const float3 smoothedPosition = lerp(m_position, gazeAtTwoMeters, deltaTime * 4.0f);

        // This will be used as the translation component of the hologram's
        // model transform.

        // 이것은 홀로그램의 모델 변환의 변환 구성 요소로 사용될 것입니다.
        SetPosition(smoothedPosition);
    }
}

// This function uses a SpatialPointerPose to position the world-locked hologram
// two meters in front of the user's heading.
// 이 함수는 SpatialPointerPose를 사용하여 사용자의 시선 방향으로부터 두 미터 앞에 월드-락 홀로그램을 위치시킵니다.
void ModelRenderer::PositionHologramNoSmoothing(SpatialPointerPose const& pointerPose)
{
    if (pointerPose != nullptr)
    {
        // Get the gaze direction relative to the given coordinate system.
        // 주어진 좌표계에 대해 시선 방향을 가져옵니다.
        const float3 headPosition = pointerPose.Head().Position();
        const float3 headDirection = pointerPose.Head().ForwardDirection();

        constexpr float distanceFromUser = 1.0f; // meters
        const float3 gazeAtTwoMeters = headPosition + (distanceFromUser * headDirection);

        // This will be used as the translation component of the hologram's
        // model transform.
        // 이것은 홀로그램의 모델 변환의 변환 구성 요소로 사용될 것입니다.
        SetPosition(gazeAtTwoMeters);
    }
}

// Called once per frame. Rotates the cube, and calculates and sets the model matrix
// relative to the position transform indicated by hologramPositionTransform.
// 매 프레임마다 호출됩니다. 큐브를 회전시키고, 홀로그램 위치 변환에 의해 지정된 위치 변환에 상대적인 모델 행렬을 계산하고 설정합니다.
void ModelRenderer::Update(DX::StepTimer const& timer)
{
    // Seconds elapsed since previous frame.
    // 이전 프레임 이후 경과한 초입니다.
    const float deltaTime = static_cast<float>(timer.GetElapsedSeconds());
    const float oneOverDeltaTime = 1.f / deltaTime;

    // Create a direction normal from the hologram's position to the origin of person space.
    // This is the z-axis rotation.
    // 홀로그램의 위치에서 사람 공간의 원점까지의 방향 법선을 생성합니다.
    // 이것은 z축 회전입니다.
    XMVECTOR facingNormal = XMVector3Normalize(-XMLoadFloat3(&m_position));

    // Rotate the x-axis around the y-axis.
    // This is a 90-degree angle from the normal, in the xz-plane.
    // This is the x-axis rotation.
    // x축을 y축 주위로 회전시킵니다.
    // 이것은 xz-평면에서 법선과 90도 각도입니다.
    // 이것은 x축 회전입니다.
    XMVECTOR xAxisRotation = XMVector3Normalize(XMVectorSet(XMVectorGetZ(facingNormal), 0.f, -XMVectorGetX(facingNormal), 0.f));

    // Create a third normal to satisfy the conditions of a rotation matrix.
    // The cross product  of the other two normals is at a 90-degree angle to 
    // both normals. (Normalize the cross product to avoid floating-point math
    // errors.)
    // Note how the cross product will never be a zero-matrix because the two normals
    // are always at a 90-degree angle from one another.
    // 회전 행렬의 조건을 만족시키는 세 번째 법선을 생성합니다.
    // 다른 두 법선의 외적은 두 법선 모두와 90도 각을 이룹니다. (부동 소수점 계산 오류를 방지하기 위해 외적을 정규화합니다.)
    // 두 법선이 항상 서로 90도 각을 이루기 때문에 외적이 절대 영 행렬이 되지 않는다는 점을 주목하세요.
    XMVECTOR yAxisRotation = XMVector3Normalize(XMVector3Cross(facingNormal, xAxisRotation));
    
    // Construct the 4x4 rotation matrix.

    // Rotate the quad to face the user.
    XMMATRIX headlockedRotationMatrix = XMMATRIX(
        xAxisRotation,
        yAxisRotation,
        facingNormal,
        XMVectorSet(0.f, 0.f, 0.f, 1.f)
        );

    XMMATRIX groupScalingMatrix = XMMatrixScaling(m_fGroupScale, m_fGroupScale, m_fGroupScale);
    XMMATRIX groupTransformMatrix = XMLoadFloat4x4(&m_groupTransform);

    XMMATRIX modelRotation = GetModelRotation();
    XMMATRIX modelTransform = XMLoadFloat4x4(&m_modelTransform);

    // Position the quad.
    const XMMATRIX headlockedTranslation = XMMatrixTranslationFromVector(XMLoadFloat3(&m_position));
    const XMMATRIX modelOffsetTranslation = XMMatrixTranslationFromVector(XMLoadFloat3(&m_offset));

    // The view and projection matrices are provided by the system; they are associated
    // with holographic cameras, and updated on a per-camera basis.
    // Here, we provide the model transform for the sample hologram. The model transform
    // matrix is transposed to prepare it for the shader.

    // headlockedRotationMatrix * headlockedTranslation transforms to a headlocked reference frame in the render coordinates system. These are then same
    // for all models.
    // (modelRotation * modelOffsetTranslation adujst the model position in its own reference frame. These are different per model.

    XMMATRIX combinedModelTransform = modelRotation * modelTransform * modelOffsetTranslation;

    XMStoreFloat4x4(&m_modelConstantBufferData.model, XMMatrixTranspose(combinedModelTransform * groupScalingMatrix * groupTransformMatrix * headlockedRotationMatrix * headlockedTranslation));

    // Loading is asynchronous. Resources must be created before they can be updated.
    if (!m_loadingComplete)
    {
        return;
    }

    // Use the D3D device context to update Direct3D device-based resources.
    const auto context = m_deviceResources->GetD3DDeviceContext();
     
    // Update the model transform buffer for the hologram.
    context->UpdateSubresource(
        m_modelConstantBuffer.Get(),
        0,
        nullptr,
        &m_modelConstantBufferData,
        0,
        0
    );
}

void ModelRenderer::SetSensorFrame(IResearchModeSensorFrame* pSensorFrame)
{

}

// Renders one frame using the vertex and pixel shaders.
// On devices that do not support the D3D11_FEATURE_D3D11_OPTIONS3::
// VPAndRTArrayIndexFromAnyShaderFeedingRasterizer optional feature,
// a pass-through geometry shader is also used to set the render 
// target array index.

// 버텍스 셰이더와 픽셀 셰이더를 사용하여 한 프레임을 렌더링합니다.
// D3D11_FEATURE_D3D11_OPTIONS3::VPAndRTArrayIndexFromAnyShaderFeedingRasterizer 선택적 기능을 지원하지 않는 장치에서는,
// 렌더 타겟 배열 인덱스를 설정하기 위해 패스-스루 지오메트리 셰이더도 사용됩니다.
void ModelRenderer::Render()
{
    // Loading is asynchronous. Resources must be created before drawing can occur.
    if (!m_loadingComplete || !m_renderEnabled)
    {
        return;
    }

    const auto context = m_deviceResources->GetD3DDeviceContext();

    // Each vertex is one instance of the VertexPositionColor struct.
    const UINT stride = sizeof(VertexPositionColor);
    const UINT offset = 0;
    context->IASetVertexBuffers(
        0,
        1,
        m_vertexBuffer.GetAddressOf(),
        &stride,
        &offset
    );
    context->IASetIndexBuffer(
        m_indexBuffer.Get(),
        DXGI_FORMAT_R16_UINT, // Each index is one 16-bit unsigned integer (short).
        0
    );
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    context->IASetInputLayout(m_inputLayout.Get());

    // Attach the vertex shader.
    context->VSSetShader(
        m_vertexShader.Get(),
        nullptr,
        0
    );

    // Set pixel shader resources
    {
        std::lock_guard<std::mutex> guard(m_mutex);

        UpdateSlateTexture();

        ID3D11ShaderResourceView* shaderResourceViews[1] =
        {
            nullptr != m_texture2D ? m_texture2D->GetShaderResourceView().Get() : nullptr
        };

        context->PSSetShaderResources(
            0 /* StartSlot */,
            1 /* NumViews */,
            shaderResourceViews);
    }

    // Apply the model constant buffer to the vertex shader.
    context->VSSetConstantBuffers(
        0,
        1,
        m_modelConstantBuffer.GetAddressOf()
    );

    if (!m_usingVprtShaders)
    {
        // On devices that do not support the D3D11_FEATURE_D3D11_OPTIONS3::
        // VPAndRTArrayIndexFromAnyShaderFeedingRasterizer optional feature,
        // a pass-through geometry shader is used to set the render target 
        // array index.
        context->GSSetShader(
            m_geometryShader.Get(),
            nullptr,
            0
        );
    }

    // Attach the pixel shader.
    context->PSSetShader(
        m_pixelShader.Get(),
        nullptr,
        0
    );

    // Draw the objects.
    context->DrawIndexedInstanced(
        m_indexCount,   // Index count per instance.
        2,              // Instance count.
        0,              // Start index location.
        0,              // Base vertex location.
        0               // Start instance location.
    );
}

std::future<void> ModelRenderer::CreateDeviceDependentResources()
{
    m_usingVprtShaders = m_deviceResources->GetDeviceSupportsVprt();

    // On devices that do support the D3D11_FEATURE_D3D11_OPTIONS3::
    // VPAndRTArrayIndexFromAnyShaderFeedingRasterizer optional feature
    // we can avoid using a pass-through geometry shader to set the render
    // target array index, thus avoiding any overhead that would be 
    // incurred by setting the geometry shader stage.
    std::wstring vertexShaderFileName = m_usingVprtShaders ? L"ms-appx:///VprtVertexShader.cso" : L"ms-appx:///VertexShader.cso";

    // Shaders will be loaded asynchronously.

    // After the vertex shader file is loaded, create the shader and input layout.
    std::vector<byte> vertexShaderFileData = co_await DX::ReadDataAsync(vertexShaderFileName);
    winrt::check_hresult(
        m_deviceResources->GetD3DDevice()->CreateVertexShader(
            vertexShaderFileData.data(),
            vertexShaderFileData.size(),
            nullptr,
            &m_vertexShader
        ));

    constexpr std::array<D3D11_INPUT_ELEMENT_DESC, 3> vertexDesc =
        { {
            { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,  0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "COLOR",    0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "TEXCOORD",    0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        } };

    winrt::check_hresult(
        m_deviceResources->GetD3DDevice()->CreateInputLayout(
            vertexDesc.data(),
            static_cast<UINT>(vertexDesc.size()),
            vertexShaderFileData.data(),
            static_cast<UINT>(vertexShaderFileData.size()),
            &m_inputLayout
        ));

    //std::wstring pixelShaderFile = L"ms-appx:///PixelShader.cso";
    // After the pixel shader file is loaded, create the shader and constant buffer.
    std::vector<byte> pixelShaderFileData = co_await DX::ReadDataAsync(m_pixelShaderFile.data());
    winrt::check_hresult(
        m_deviceResources->GetD3DDevice()->CreatePixelShader(
            pixelShaderFileData.data(),
            pixelShaderFileData.size(),
            nullptr,
            &m_pixelShader
        ));

    const CD3D11_BUFFER_DESC constantBufferDesc(sizeof(ModelConstantBuffer), D3D11_BIND_CONSTANT_BUFFER);
    winrt::check_hresult(
        m_deviceResources->GetD3DDevice()->CreateBuffer(
            &constantBufferDesc,
            nullptr,
            &m_modelConstantBuffer
        ));


    if (!m_usingVprtShaders)
    {
        // Load the pass-through geometry shader.
        std::vector<byte> geometryShaderFileData = co_await DX::ReadDataAsync(L"ms-appx:///GeometryShader.cso");

        // After the pass-through geometry shader file is loaded, create the shader.
        winrt::check_hresult(
            m_deviceResources->GetD3DDevice()->CreateGeometryShader(
                geometryShaderFileData.data(),
                geometryShaderFileData.size(),
                nullptr,
                &m_geometryShader
            ));
    }

    std::vector<VertexPositionColor> modelVertices;
    GetModelVertices(modelVertices);

    D3D11_SUBRESOURCE_DATA vertexBufferData = { 0 };
    vertexBufferData.pSysMem = modelVertices.data();
    vertexBufferData.SysMemPitch = 0;
    vertexBufferData.SysMemSlicePitch = 0;
    const CD3D11_BUFFER_DESC vertexBufferDesc(sizeof(VertexPositionColor) * static_cast<UINT>(modelVertices.size()), D3D11_BIND_VERTEX_BUFFER);
    winrt::check_hresult(
        m_deviceResources->GetD3DDevice()->CreateBuffer(
            &vertexBufferDesc,
            &vertexBufferData,
            &m_vertexBuffer
        ));


    std::vector<unsigned short> modelIndices;
    GetModelTriangleIndices(modelIndices);

    m_indexCount = static_cast<unsigned int>(modelIndices.size());

    D3D11_SUBRESOURCE_DATA indexBufferData = { 0 };
    indexBufferData.pSysMem = modelIndices.data();
    indexBufferData.SysMemPitch = 0;
    indexBufferData.SysMemSlicePitch = 0;
    CD3D11_BUFFER_DESC indexBufferDesc(sizeof(unsigned short) * static_cast<UINT>(modelIndices.size()), D3D11_BIND_INDEX_BUFFER);
    winrt::check_hresult(
        m_deviceResources->GetD3DDevice()->CreateBuffer(
            &indexBufferDesc,
            &indexBufferData,
            &m_indexBuffer
        ));

    // Once the cube is loaded, the object is ready to be rendered.
    m_loadingComplete = true;
}

void ModelRenderer::ReleaseDeviceDependentResources()
{
    m_loadingComplete = false;
    m_usingVprtShaders = false;
    m_vertexShader.Reset();
    m_inputLayout.Reset();
    m_pixelShader.Reset();
    m_geometryShader.Reset();
    m_modelConstantBuffer.Reset();
    m_vertexBuffer.Reset();
    m_indexBuffer.Reset();
}
