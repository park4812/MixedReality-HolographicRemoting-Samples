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

#include "SamplePlayerMain.h"

#include "../common/Content/DDSTextureLoader.h"
#include "../common/PlayerUtil.h"

#include <sstream>

#include <winrt/Windows.Foundation.Metadata.h>
#include <winrt/Windows.Ui.Popups.h>

#include "SensorVisualizationScenario.h"

#include <fstream>

#include <winrt/Windows.Storage.h>
#include <winrt/Windows.Storage.Streams.h>

#include <VuforiaEngine/VuforiaEngine.h>
#include <VuforiaEngine/Engine/Engine.h>
#include <VuforiaEngine/Engine/UWP/PlatformConfig_UWP.h>

#include <winrt/Windows.UI.Core.h>

using namespace winrt;
using namespace winrt::Windows::Storage;
using namespace winrt::Windows::Storage::Streams;
using namespace std::chrono_literals;

using namespace winrt::Microsoft::Holographic::AppRemoting;
using namespace winrt::Windows::ApplicationModel;
using namespace winrt::Windows::ApplicationModel::Activation;
using namespace winrt::Windows::ApplicationModel::Core;
using namespace winrt::Windows::Foundation::Numerics;
using namespace winrt::Windows::Graphics::Holographic;
using namespace winrt::Windows::Graphics::DirectX::Direct3D11;
using namespace winrt::Windows::Perception::Spatial;
using namespace winrt::Windows::UI::Core;
using namespace winrt::Windows::UI::Input::Spatial;


extern "C"
HMODULE LoadLibraryA(
    LPCSTR lpLibFileName
);

namespace
{
    constexpr int64_t s_loadingDotsMaxCount = 3;
    // clang-format off
    constexpr char licenseKey[] = "AVHCLvP/////AAABmWwC7IbDoUN6mYq1HaXv5GwPBwAYQsVp8szql79Z8g9Lm3U+mmWO5I3tpb65cc5lGs+7kndf3N/UGIzXLSIH6G1LfAelOWeZBhRS7bmdwfoe5Qe4IJa3Zb8Y4tRJyBknHh//vPjyTfh6V5B1hwKH8iB2JNULWVLvwjLDAm3C57BTcs5oHGo3+7Z8zma8RaE4lycngdPmLgSLQjX8s0rOGMr8xthCP7Th8hSK+D2kzVdJtZCXN2/UgGOn/kFW5r/w+vZIgnpwq5rYtGOlawAq2lBGFQyHKYUabTnqUipHMvUzNe17Z3yQlWvLWoy6VhBlyiPQTSmv8oOzaKC44Qx57Ai49+ou0f6Kcgfw2DrGvISC";
    // clang-format on

    constexpr float NEAR_PLANE = 0.01f;
    constexpr float FAR_PLANE = 5.f;
}

/// Helper macro to check results of Vuforia Engine calls that are expected to succeed
#define REQUIRE_SUCCESS(command)                                                                                                           \
    {                                                                                                                                      \
        auto vu_result_appsupport_ = command;                                                                                              \
        (void)vu_result_appsupport_;                                                                                                       \
        assert(vu_result_appsupport_ == VU_SUCCESS);                                                                                       \
    }


SamplePlayerMain::SamplePlayerMain()
{
    m_canCommitDirect3D11DepthBuffer = winrt::Windows::Foundation::Metadata::ApiInformation::IsMethodPresent(
        L"Windows.Graphics.Holographic.HolographicCameraRenderingParameters", L"CommitDirect3D11DepthBuffer");

    m_ipAddressUpdater = CreateIpAddressUpdater();
}

SamplePlayerMain::~SamplePlayerMain()
{
    Uninitialize();
}

void SamplePlayerMain::ConnectOrListen()
{
    // Disconnect from a potentially existing connection first
    m_playerContext.Disconnect();

    UpdateStatusDisplay();
    //m_playerOptions.m_listen = false;
    //winrt::hstring ip_address = L"192.168.0.142";
    //m_playerOptions.m_hostname = ip_address;
    // Try to establish a connection as specified in m_playerOptions
    try
    {
        // Fallback to default port 8265, in case no valid port number was specified
        const uint16_t port = (m_playerOptions.m_port != 0) ? m_playerOptions.m_port : 8265;

        if (m_playerOptions.m_listen)
        {
            // Put the PlayerContext in network server mode. In this mode the player listens for an incoming network connection.
            // The hostname specifies the local address on which the player listens on.
            // Use the port as the handshake port (where clients always connect to first), and port + 1 for the
            // primary transport implementation (clients are redirected to this port as part of the handshake).
            // PlayerContext를 네트워크 서버 모드로 설정합니다. 이 모드에서 플레이어는 들어오는 네트워크 연결을 대기합니다.
            // 호스트 이름은 플레이어가 연결을 대기하는 로컬 주소를 지정합니다.
            // 포트를 핸드셰이크 포트로 사용하세요(클라이언트가 먼저 연결하는 포트), 그리고 포트 + 1을 기본 전송 구현 포트로
            // 사용하세요(클라이언트는 핸드셰이크의 일부로 이 포트로 리디렉션됩니다).
            m_playerContext.Listen(m_playerOptions.m_hostname, port, port + 1);
            //m_playerContext.Listen(ip_address, 8265, 8266);
        }
        else
        {
            // Put the PlayerContext in network client mode.
            // In this mode the player tries to establish a network connection to the provided hostname at the given port.
            // The port specifies the server's handshake port. The primary transport port will be specified by the server as part of the
            // handshake.
            // PlayerContext를 네트워크 클라이언트 모드로 설정합니다.
            // 이 모드에서 플레이어는 주어진 포트에서 제공된 호스트 이름으로 네트워크 연결을 설정하려고 시도합니다.
            // 포트는 서버의 핸드셰이크 포트를 지정합니다. 기본 전송 포트는 핸드셰이크의 일부로 서버에 의해 지정될 것입니다.
            m_playerContext.Connect(m_playerOptions.m_hostname, port);
            //m_playerContext.Connect(ip_address, 8265);
        }
    }
    catch (winrt::hresult_error& ex)
    {
        // If Connect/Listen fails, display the error message
        // Possible reasons for this are invalid parameters or because the PlayerContext is already in connected or connecting state.
        m_errorHelper.AddError(
            std::wstring(m_playerOptions.m_listen ? L"Failed to Listen: " : L"Failed to Connect: ") + std::wstring(ex.message().c_str()));
        ConnectOrListenAfter(1s);
    }

    UpdateStatusDisplay();
}

winrt::fire_and_forget SamplePlayerMain::ConnectOrListenAfter(std::chrono::system_clock::duration time)
{
    // Get a weak reference before switching to a background thread.
    auto weakThis = get_weak();

    // Continue after the given time in a background thread
    using namespace winrt;
    co_await time;

    // Return if the player has been destroyed in the meantime
    auto strongThis = weakThis.get();
    if (!strongThis)
    {
        co_return;
    }

    // Try to connect or listen
    ConnectOrListen();
}


int fileint = 0;
int cnt = 0;


void SaveDataToFile(const BYTE* pData, size_t size)
{
    std::ofstream file(fileint++ + ".raw", std::ios::binary);
    if (file.is_open())
    {
        file.write(reinterpret_cast<const char*>(pData), size);
        file.close();
    }
}

winrt::fire_and_forget SaveDataToFile1(const BYTE* pData, size_t size)
{
    auto folder = ApplicationData::Current().LocalFolder();

    //StorageFolder picturesLibrary = KnownFolders::PicturesLibrary();
    std::wstring fileName = std::to_wstring(fileint++) + L".raw"; // 파일 이름 생성
    StorageFile file = co_await folder.CreateFileAsync(fileName, CreationCollisionOption::ReplaceExisting);

    // 파일에 데이터 쓰기
    auto stream = co_await file.OpenAsync(FileAccessMode::ReadWrite);
    auto outputStream = stream.GetOutputStreamAt(0);
    auto dataWriter = DataWriter(outputStream);
    dataWriter.WriteBytes(array_view<const uint8_t>(pData, pData + size));
    co_await dataWriter.StoreAsync();
    co_await outputStream.FlushAsync();

    stream.Close();     // 스트림 닫기
    dataWriter.Close(); // 데이터 라이터 닫기
}


winrt::fire_and_forget SaveDataToFile1(const UINT16* pData, size_t size)
{
    auto folder = ApplicationData::Current().LocalFolder();
    std::wstring fileName = std::to_wstring(fileint++) + L"1.raw"; // 파일 이름 생성
    StorageFile file = co_await folder.CreateFileAsync(fileName, CreationCollisionOption::ReplaceExisting);

    // 파일에 데이터 쓰기
    auto stream = co_await file.OpenAsync(FileAccessMode::ReadWrite);
    auto outputStream = stream.GetOutputStreamAt(0);
    auto dataWriter = DataWriter(outputStream);

    // UINT16 데이터를 byte 배열로 해석하여 쓰기
    // pData 포인터를 byte 배열로 취급하고 전체 크기를 byte로 계산합니다.
    dataWriter.WriteBytes(array_view<const uint8_t>(
        reinterpret_cast<const uint8_t*>(pData), reinterpret_cast<const uint8_t*>(pData) + (size * sizeof(UINT16))));

    co_await dataWriter.StoreAsync();
    co_await outputStream.FlushAsync();

    stream.Close();     // 스트림 닫기
    dataWriter.Close(); // 데이터 라이터 닫기
}


winrt::fire_and_forget SaveDataToFile1()
{
    auto folder = ApplicationData::Current().LocalFolder();

    // StorageFolder picturesLibrary = KnownFolders::PicturesLibrary();
    std::wstring fileName = std::to_wstring(fileint++) + L"fail.raw"; // 파일 이름 생성
    StorageFile file = co_await folder.CreateFileAsync(fileName, CreationCollisionOption::ReplaceExisting);

    // 파일에 데이터 쓰기
    auto stream = co_await file.OpenAsync(FileAccessMode::ReadWrite);
    auto outputStream = stream.GetOutputStreamAt(0);
    auto dataWriter = DataWriter(outputStream);
    dataWriter.WriteByte('d');
    co_await dataWriter.StoreAsync();
    co_await outputStream.FlushAsync();

    stream.Close();     // 스트림 닫기
    dataWriter.Close(); // 데이터 라이터 닫기
}


winrt::fire_and_forget SaveDataToFile2()
{
    auto folder = ApplicationData::Current().LocalFolder();

    // StorageFolder picturesLibrary = KnownFolders::PicturesLibrary();
    std::wstring fileName = L"run.raw"; // 파일 이름 생성
    StorageFile file = co_await folder.CreateFileAsync(fileName, CreationCollisionOption::ReplaceExisting);

    // 파일에 데이터 쓰기
    auto stream = co_await file.OpenAsync(FileAccessMode::ReadWrite);
    auto outputStream = stream.GetOutputStreamAt(0);
    auto dataWriter = DataWriter(outputStream);
    dataWriter.WriteByte('r');
    co_await dataWriter.StoreAsync();
    co_await outputStream.FlushAsync();

    stream.Close();     // 스트림 닫기
    dataWriter.Close(); // 데이터 라이터 닫기
}

void GetSensorData(IResearchModeSensorFrame* pSensorFrame)
{

    IResearchModeSensorVLCFrame* pVLCFrame = nullptr;
    IResearchModeSensorDepthFrame* pDepthFrame = nullptr;


    HRESULT hr = pSensorFrame->QueryInterface(IID_PPV_ARGS(&pVLCFrame));

    if (FAILED(hr))
    {
        const UINT16* pIntBuffer = nullptr;
        size_t bufferSize = 0;

        hr = pSensorFrame->QueryInterface(IID_PPV_ARGS(&pDepthFrame));

        pDepthFrame->GetBuffer(&pIntBuffer, &bufferSize);

        SaveDataToFile1(pIntBuffer, bufferSize);

        pDepthFrame->Release();
    }
    else if (SUCCEEDED(hr) && pVLCFrame)
    {
        // 센서 특정 데이터 처리
        const BYTE* pImageBuffer = nullptr;
        size_t bufferSize = 0;

        pVLCFrame->GetBuffer(&pImageBuffer, &bufferSize);

        // 여기서 파일로 저장
        SaveDataToFile1(pImageBuffer, bufferSize);

        pVLCFrame->Release();
    }
    else
    {
        SaveDataToFile1();
    }
}


HolographicFrame SamplePlayerMain::Update(float deltaTimeInSeconds, const HolographicFrame& prevHolographicFrame)
{
    SpatialCoordinateSystem focusPointCoordinateSystem = nullptr;
    float3 focusPointPosition{0.0f, 0.0f, 0.0f};

    // Update the position of the status and error display.
    // Note, this is done with the data from the previous frame before the next wait to save CPU time and get the remote frame presented as
    // fast as possible. This also means that focus point and status display position are one frame behind which is a reasonable tradeoff
    // for the time we win.
    // 상태 및 오류 디스플레이의 위치를 업데이트합니다.
    // 참고로, 이 작업은 CPU 시간을 절약하고 리모트 프레임을 가능한 빨리 표시하기 위해 다음 대기 전에 이전 프레임의 데이터로 수행됩니다.
    // 이는 포커스 포인트와 상태 디스플레이 위치가 한 프레임 뒤쳐지는 것을 의미하는데, 이는 우리가 얻는 시간을 위한 합리적인 타협입니다.

    if (prevHolographicFrame != nullptr && m_attachedFrameOfReference != nullptr)
    {
        HolographicFramePrediction prevPrediction = prevHolographicFrame.CurrentPrediction();
        SpatialCoordinateSystem coordinateSystem =
            m_attachedFrameOfReference.GetStationaryCoordinateSystemAtTimestamp(prevPrediction.Timestamp());

        auto poseIterator = prevPrediction.CameraPoses().First();
        if (poseIterator.HasCurrent())
        {
            HolographicCameraPose cameraPose = poseIterator.Current();
            if (auto visibleFrustumReference = cameraPose.TryGetVisibleFrustum(coordinateSystem))
            {
                const float imageOffsetX = m_trackingLost ? -0.0095f : -0.0125f;
                const float imageOffsetY = 0.0111f;
                m_statusDisplay->PositionDisplay(deltaTimeInSeconds, visibleFrustumReference.Value(), imageOffsetX, imageOffsetY);
            }
        }

        focusPointCoordinateSystem = coordinateSystem;
        focusPointPosition = m_statusDisplay->GetPosition();
    }

    // Update content of the status and error display.
    // 상태 및 오류 디스플레이의 내용을 업데이트합니다.
    {
        // Update the accumulated statistics with the statistics from the last frame.
        // 마지막 프레임에서의 통계로 누적 통계를 업데이트합니다.
        m_statisticsHelper.Update(m_playerContext.LastFrameStatistics());

        const bool updateStats = m_statisticsHelper.StatisticsHaveChanged() && m_playerOptions.m_showStatistics;
        if (updateStats || !m_firstRemoteFrameWasBlitted)
        {
            UpdateStatusDisplay();
        }

        const bool connected = (m_playerContext.ConnectionState() == ConnectionState::Connected);
        if (!(connected && !m_trackingLost))
        {
            if (m_playerOptions.m_listen)
            {
                auto deviceIpNew = m_ipAddressUpdater->GetIpAddress(m_playerOptions.m_ipv6);
                if (m_deviceIp != deviceIpNew)
                {
                    m_deviceIp = deviceIpNew;

                    UpdateStatusDisplay();
                }
            }
        }
        if (connected && cnt++ == 1000)
        {
            cnt = 0;
            IResearchModeSensorFrame* pSensorFrame = nullptr;
            ResearchModeSensorTimestamp timeStamp;

            winrt::check_hresult(m_pLTSensor->GetNextBuffer(&pSensorFrame));
            pSensorFrame->GetTimeStamp(&timeStamp);
            GetSensorData(pSensorFrame);

        }
        m_statusDisplay->SetImageEnabled(!connected);
        m_statusDisplay->Update(deltaTimeInSeconds);
        m_errorHelper.Update(deltaTimeInSeconds, [this]() { UpdateStatusDisplay(); });
    }

    HolographicFrame holographicFrame = m_deviceResources->GetHolographicSpace().CreateNextFrame();
    {
        // Note, we don't wait for the next frame on present which allows us to first update all view independent stuff and also create the
        // next frame before we actually wait. By doing so everything before the wait is executed while the previous frame is presented by
        // the OS and thus saves us quite some CPU time after the wait.
        // 참고로, 다음 프레임을 표시할 때까지 기다리지 않습니다. 이를 통해 먼저 모든 뷰와 독립적인 요소들을 업데이트하고 실제로 기다리기 전에
        // 다음 프레임을 생성할 수 있습니다. 이렇게 함으로써 기다리기 전에 모든 작업이 실행되며 이전 프레임이 운영체제에 의해 표시되는 동안
        // 상당한 CPU 시간을 절약할 수 있습니다.

        m_deviceResources->WaitForNextFrameReady();
    }
    holographicFrame.UpdateCurrentPrediction();

    // Back buffers can change from frame to frame. Validate each buffer, and recreate resource views and depth buffers as needed.
    m_deviceResources->EnsureCameraResources(
        holographicFrame, holographicFrame.CurrentPrediction(), focusPointCoordinateSystem, focusPointPosition);

#ifdef ENABLE_USER_COORDINATE_SYSTEM_SAMPLE
    if (m_playerContext.ConnectionState() == ConnectionState::Connected && !m_trackingLost && m_userSpatialFrameOfReference != nullptr)
    {
        SpatialCoordinateSystem userCoordinateSystem = m_userSpatialFrameOfReference.CoordinateSystem();

        try
        {
            m_playerContext.UpdateUserSpatialFrameOfReference(userCoordinateSystem);
        }
        catch (...)
        {
        }

        SpatialCoordinateSystem renderingCoordinateSystem =
            m_attachedFrameOfReference.GetStationaryCoordinateSystemAtTimestamp(holographicFrame.CurrentPrediction().Timestamp());
        m_simpleCubeRenderer->Update(renderingCoordinateSystem, userCoordinateSystem);
    }
#endif

    return holographicFrame;
}

void SamplePlayerMain::Render(const HolographicFrame& holographicFrame)
{
    bool atLeastOneCameraRendered = false;

    m_deviceResources->UseHolographicCameraResources(
        [this, holographicFrame, &atLeastOneCameraRendered](
            std::map<UINT32, std::unique_ptr<DXHelper::CameraResourcesD3D11Holographic>>& cameraResourceMap) {
            HolographicFramePrediction prediction = holographicFrame.CurrentPrediction();

            SpatialCoordinateSystem coordinateSystem = nullptr;
            if (m_attachedFrameOfReference)
            {
                coordinateSystem = m_attachedFrameOfReference.GetStationaryCoordinateSystemAtTimestamp(prediction.Timestamp());
            }

            // Retrieve information about any pending render target size change requests
            bool needRenderTargetSizeChange = false;
            winrt::Windows::Foundation::Size newRenderTargetSize{};
            {
                std::lock_guard lock{m_renderTargetSizeChangeMutex};
                if (m_needRenderTargetSizeChange)
                {
                    needRenderTargetSizeChange = true;
                    newRenderTargetSize = m_newRenderTargetSize;
                    m_needRenderTargetSizeChange = false;
                }
            }

            for (const HolographicCameraPose& cameraPose : prediction.CameraPoses())
            {
                DXHelper::CameraResourcesD3D11Holographic* pCameraResources = cameraResourceMap[cameraPose.HolographicCamera().Id()].get();

                m_deviceResources->UseD3DDeviceContext([&](ID3D11DeviceContext3* deviceContext) {
                    ID3D11DepthStencilView* depthStencilView = pCameraResources->GetDepthStencilView();

                    // Set render targets to the current holographic camera.
                    ID3D11RenderTargetView* const targets[1] = {pCameraResources->GetBackBufferRenderTargetView()};
                    deviceContext->OMSetRenderTargets(1, targets, depthStencilView);

                    if (!targets[0] || !depthStencilView)
                    {
                        return;
                    }

                    if (coordinateSystem)
                    {
                        // The view and projection matrices for each holographic camera will change
                        // every frame. This function refreshes the data in the constant buffer for
                        // the holographic camera indicated by cameraPose.
                        pCameraResources->UpdateViewProjectionBuffer(m_deviceResources, cameraPose, coordinateSystem);

                        const bool connected = (m_playerContext.ConnectionState() == ConnectionState::Connected);

                        // Reduce the fov of the statistics view.
                        bool useLandscape =
                            m_playerOptions.m_showStatistics && connected && !m_trackingLost && m_firstRemoteFrameWasBlitted;

                        // Pass data from the camera resources to the status display.
                        m_statusDisplay->UpdateTextScale(
                            pCameraResources->GetProjectionTransform(),
                            pCameraResources->GetRenderTargetSize().Width,
                            pCameraResources->GetRenderTargetSize().Height,
                            useLandscape,
                            pCameraResources->IsOpaque());
                    }

                    // Attach the view/projection constant buffer for this camera to the graphics pipeline.
                    bool cameraActive = pCameraResources->AttachViewProjectionBuffer(m_deviceResources);

                    // Only render world-locked content when positional tracking is active.
                    if (cameraActive)
                    {
                        auto blitResult = BlitResult::Failed_NoRemoteFrameAvailable;

                        try
                        {
                            if (m_playerContext.ConnectionState() == ConnectionState::Connected)
                            {
                                // Blit the remote frame into the backbuffer for the HolographicFrame.
                                // NOTE: This overwrites the focus point for the current frame, if the remote application
                                // has specified a focus point during the rendering of the remote frame.
                                blitResult = m_playerContext.BlitRemoteFrame();
                            }
                        }
                        catch (winrt::hresult_error err)
                        {
                            winrt::hstring msg = err.message();
                            m_errorHelper.AddError(std::wstring(L"BlitRemoteFrame failed: ") + msg.c_str());
                            UpdateStatusDisplay();
                        }

                        // If a remote remote frame has been blitted then color and depth buffer are fully overwritten, otherwise we have to
                        // clear both buffers before we render any local content.
                        if (blitResult != BlitResult::Success_Color && blitResult != BlitResult::Success_Color_Depth)
                        {
                            // Clear the back buffer and depth stencil view.
                            deviceContext->ClearRenderTargetView(targets[0], DirectX::Colors::Transparent);
                            deviceContext->ClearDepthStencilView(depthStencilView, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
                        }
                        else
                        {
                            m_firstRemoteFrameWasBlitted = true;
                            UpdateStatusDisplay();
                        }

                        // Render local content.
                        {
                        // NOTE: Any local custom content would be rendered here.
#ifdef ENABLE_USER_COORDINATE_SYSTEM_SAMPLE
                            if (m_playerContext.ConnectionState() == ConnectionState::Connected)
                            {
                                // Draw the cube.
                                m_simpleCubeRenderer->Render(pCameraResources->IsRenderingStereoscopic());
                            }
#endif
                            // Draw connection status and/or statistics.
                            m_statusDisplay->Render();
                        }

                        // Commit depth buffer if it has been committed by the remote app which is indicated by Success_Color_Depth.
                        // NOTE: CommitDirect3D11DepthBuffer should be the last thing before the frame is presented. By doing so the depth
                        //       buffer submitted includes remote content and local content.
                        if (m_canCommitDirect3D11DepthBuffer && blitResult == BlitResult::Success_Color_Depth)
                        {
                            auto interopSurface = pCameraResources->GetDepthStencilTextureInteropObject();
                            HolographicCameraRenderingParameters renderingParameters = holographicFrame.GetRenderingParameters(cameraPose);
                            renderingParameters.CommitDirect3D11DepthBuffer(interopSurface);
                        }
                    }

                    atLeastOneCameraRendered = true;
                });

                if (needRenderTargetSizeChange)
                {
                    if (HolographicViewConfiguration viewConfig = cameraPose.HolographicCamera().ViewConfiguration())
                    {
                        // Only request new render target size if we are dealing with an opaque (i.e., VR) display
                        if (cameraPose.HolographicCamera().Display().IsOpaque())
                        {
                            viewConfig.RequestRenderTargetSize(newRenderTargetSize);
                        }
                    }
                }
            }
        });

    if (atLeastOneCameraRendered)
    {
        m_deviceResources->Present(holographicFrame);
    }
}

#pragma region IFrameworkViewSource methods

IFrameworkView SamplePlayerMain::CreateView()
{
    return *this;
}

#pragma endregion IFrameworkViewSource methods

#pragma region IFrameworkView methods

static ResearchModeSensorConsent camAccessCheck;
static HANDLE camConsentGiven;
static ResearchModeSensorConsent imuAccessCheck;
static HANDLE imuConsentGiven;

void CamAccessOnComplete(ResearchModeSensorConsent consent)
{
    camAccessCheck = consent;
    SetEvent(camConsentGiven);
}

void ImuAccessOnComplete(ResearchModeSensorConsent consent)
{
    imuAccessCheck = consent;
    SetEvent(imuConsentGiven);
}

void SamplePlayerMain::test()
{
    while (true) // 무한 루프로 지속적 실행
    {
        IResearchModeSensorFrame* pSensorFrame = nullptr;
        ResearchModeSensorTimestamp timeStamp;

        winrt::check_hresult(m_pLTSensor->GetNextBuffer(&pSensorFrame));

        if (pSensorFrame != nullptr)
        {
            pSensorFrame->GetTimeStamp(&timeStamp);
            // StatusDisplay::Line lines[] = {StatusDisplay::Line{L"Not Nullptr", StatusDisplay::Small, StatusDisplay::Yellow, 1.0f}};
        }
        else
        {

            // StatusDisplay::Line lines[] = {StatusDisplay::Line{L"Nullptr", StatusDisplay::Small, StatusDisplay::Yellow, 1.0f}};
        }

        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}
void SamplePlayerMain::Initialize(const CoreApplicationView& applicationView)
{
    // Create the player context
    // IMPORTANT: This must be done before creating the HolographicSpace (or any other call to the Holographic API).
    // 플레이어 컨텍스트 생성
    // 중요: 이 작업은 홀로그래픽 공간(또는 홀로그래픽 API에 대한 다른 호출)을 생성하기 전에 수행되어야 합니다.

    try
    {
        // 홀로그래픽 디스플레이 장치로 스트리밍하는 기술
        m_playerContext = PlayerContext::Create();

        HRESULT hr = S_OK;
        size_t sensorCount = 0;
        camConsentGiven = CreateEvent(nullptr, true, false, nullptr);
        imuConsentGiven = CreateEvent(nullptr, true, false, nullptr);

        HMODULE hrResearchMode = LoadLibraryA("ResearchModeAPI");
        if (hrResearchMode)
        {
            typedef HRESULT(__cdecl * PFN_CREATEPROVIDER)(IResearchModeSensorDevice * *ppSensorDevice);
            PFN_CREATEPROVIDER pfnCreate =
                reinterpret_cast<PFN_CREATEPROVIDER>(GetProcAddress(hrResearchMode, "CreateResearchModeSensorDevice"));
            if (pfnCreate)
            {
                winrt::check_hresult(pfnCreate(&m_pSensorDevice));
            }
            else
            {
                winrt::check_hresult(E_INVALIDARG);
            }
        }

        winrt::check_hresult(m_pSensorDevice->QueryInterface(IID_PPV_ARGS(&m_pSensorDeviceConsent)));
        winrt::check_hresult(m_pSensorDeviceConsent->RequestCamAccessAsync(CamAccessOnComplete));
        winrt::check_hresult(m_pSensorDeviceConsent->RequestIMUAccessAsync(ImuAccessOnComplete));

        m_pSensorDevice->DisableEyeSelection();

        winrt::check_hresult(m_pSensorDevice->GetSensorCount(&sensorCount));
        m_sensorDescriptors.resize(sensorCount);

        winrt::check_hresult(m_pSensorDevice->GetSensorDescriptors(m_sensorDescriptors.data(), m_sensorDescriptors.size(), &sensorCount));

        for (auto sensorDescriptor : m_sensorDescriptors)
        {
            IResearchModeSensor* pSensor = nullptr;

            if (sensorDescriptor.sensorType == LEFT_FRONT)
            {
                winrt::check_hresult(m_pSensorDevice->GetSensor(sensorDescriptor.sensorType, &m_pLFCameraSensor));
            }

            if (sensorDescriptor.sensorType == RIGHT_FRONT)
            {
                winrt::check_hresult(m_pSensorDevice->GetSensor(sensorDescriptor.sensorType, &m_pRFCameraSensor));

                int a = 3;
            }

// Long throw and AHAT modes can not be used at the same time.
#define DEPTH_USE_LONG_THROW

#ifdef DEPTH_USE_LONG_THROW
            if (sensorDescriptor.sensorType == DEPTH_LONG_THROW)
            {
                winrt::check_hresult(m_pSensorDevice->GetSensor(sensorDescriptor.sensorType, &m_pLTSensor));
                m_pSensorDevice->AddRef();

                winrt::check_hresult(m_pLTSensor->OpenStream());

                SaveDataToFile2();
            }
#else
            if (sensorDescriptor.sensorType == DEPTH_AHAT)
            {
                winrt::check_hresult(m_pSensorDevice->GetSensor(sensorDescriptor.sensorType, &m_pAHATSensor));
            }
#endif
            if (sensorDescriptor.sensorType == IMU_ACCEL)
            {
                winrt::check_hresult(m_pSensorDevice->GetSensor(sensorDescriptor.sensorType, &m_pAccelSensor));
            }
            if (sensorDescriptor.sensorType == IMU_GYRO)
            {
                winrt::check_hresult(m_pSensorDevice->GetSensor(sensorDescriptor.sensorType, &m_pGyroSensor));
            }
            if (sensorDescriptor.sensorType == IMU_MAG)
            {
                winrt::check_hresult(m_pSensorDevice->GetSensor(sensorDescriptor.sensorType, &m_pMagSensor));
            }
        }
    }
    catch (winrt::hresult_error)
    {
        // If we get here, it is likely that no Windows Holographic is installed.
        // 여기에 도달했다면, 윈도우 홀로그래픽이 설치되어 있지 않을 가능성이 높습니다.

        m_failedToCreatePlayerContext = true;
        // Return right away to avoid bringing down the application. This allows us to
        // later provide feedback to users about this failure.
        // 애플리케이션을 종료하지 않도록 즉시 반환합니다.
        // 이를 통해 나중에 사용자에게 이 실패에 대한 피드백을 제공할 수 있습니다.

        return;
    }

    // Register to the PlayerContext connection events
    // PlayerContext 연결 이벤트에 등록

    m_playerContext.OnConnected({this, &SamplePlayerMain::OnConnected});
    m_playerContext.OnDisconnected({this, &SamplePlayerMain::OnDisconnected});
    m_playerContext.OnRequestRenderTargetSize({this, &SamplePlayerMain::OnRequestRenderTargetSize});

    // Set the BlitRemoteFrame timeout to 0.5s
    // BlitRemoteFrame 타임아웃을 0.5초로 설정합니다.

    m_playerContext.BlitRemoteFrameTimeout(500ms);

    // Projection transform always reflects what has been configured on the remote side.
    // 프로젝션 변환은 항상 원격 측에서 설정된 내용을 반영합니다.

    m_playerContext.ProjectionTransformConfig(ProjectionTransformMode::Remote);

    // Enable 10% overRendering with 10% resolution increase. With this configuration, the viewport gets increased by 5% in each direction
    // and the DPI remains equal.
    // 10%의 해상도 증가로 10% 오버렌더링을 활성화합니다.
    // 이 설정을 통해 뷰포트는 각 방향으로 5%씩 증가하고 DPI는 동일하게 유지됩니다.

    OverRenderingConfig overRenderingConfig;
    overRenderingConfig.HorizontalViewportIncrease = 0.1f;
    overRenderingConfig.VerticalViewportIncrease = 0.1f;
    overRenderingConfig.HorizontalResolutionIncrease = 0.1f;
    overRenderingConfig.VerticalResolutionIncrease = 0.1f;
    m_playerContext.ConfigureOverRendering(overRenderingConfig);

    // Register event handlers for app lifecycle.
    // 앱 생명주기에 대한 이벤트 핸들러를 등록합니다.

    //일시정지일때 발생하는 이벤트
    m_suspendingEventRevoker = CoreApplication::Suspending(winrt::auto_revoke, {this, &SamplePlayerMain::OnSuspending});

    //활성화될때 발생하는 이벤트
    m_viewActivatedRevoker = applicationView.Activated(winrt::auto_revoke, {this, &SamplePlayerMain::OnViewActivated});

    m_deviceResources = std::make_shared<DXHelper::DeviceResourcesD3D11Holographic>();
    m_deviceResources->RegisterDeviceNotify(this);

    m_spatialLocator = SpatialLocator::GetDefault();
    if (m_spatialLocator != nullptr)
    {
        m_locatabilityChangedRevoker =
            m_spatialLocator.LocatabilityChanged(winrt::auto_revoke, {this, &SamplePlayerMain::OnLocatabilityChanged});
        m_attachedFrameOfReference = m_spatialLocator.CreateAttachedFrameOfReferenceAtCurrentHeading();

#ifdef ENABLE_USER_COORDINATE_SYSTEM_SAMPLE
        // Create a stationaryFrameOfReference in front of the user.
        m_userSpatialFrameOfReference =
            m_spatialLocator.CreateStationaryFrameOfReferenceAtCurrentLocation(float3(0.5f, 0.0f, -2.0f), quaternion(0, 0, 0, 1), 0.0);
#endif
    }
}


void SamplePlayerMain::SetWindow(const CoreWindow& window)
{
    m_windowVisible = window.Visible();

    m_windowClosedEventRevoker = window.Closed(winrt::auto_revoke, {this, &SamplePlayerMain::OnWindowClosed});
    m_visibilityChangedEventRevoker = window.VisibilityChanged(winrt::auto_revoke, {this, &SamplePlayerMain::OnVisibilityChanged});

    // We early out if we have no device resources here to avoid bringing down the application.
    // The reason for this is that we want to be able to provide feedback to users later on in
    // case the player context could not be created.
    if (!m_deviceResources)
    {
        return;
    }

    // Create the HolographicSpace and forward the window to the device resources.
    m_deviceResources->SetHolographicSpace(HolographicSpace::CreateForCoreWindow(window));

    // Initialize the status display.
    m_statusDisplay = std::make_unique<StatusDisplay>(m_deviceResources);

#ifdef ENABLE_USER_COORDINATE_SYSTEM_SAMPLE
    float3 simpleCubePosition = {0.0f, 0.0f, 0.0f};
    float3 simpleCubeColor = {0.0f, 0.0f, 1.0f};
    m_simpleCubeRenderer = std::make_unique<SimpleCubeRenderer>(m_deviceResources, simpleCubePosition, simpleCubeColor);
#endif
    LoadLogoImage();

#ifdef ENABLE_CUSTOM_DATA_CHANNEL_SAMPLE
    try
    {
        m_playerContext.OnDataChannelCreated([weakThis = get_weak()](const IDataChannel& dataChannel, uint8_t channelId) {
            if (auto strongThis = weakThis.get())
            {
                std::lock_guard lock(strongThis->m_customDataChannelLock);
                strongThis->m_customDataChannel = dataChannel.as<IDataChannel2>();

                strongThis->m_customChannelDataReceivedEventRevoker = strongThis->m_customDataChannel.OnDataReceived(
                    winrt::auto_revoke, [weakThis](winrt::array_view<const uint8_t> dataView) {
                        if (auto strongThis = weakThis.get())
                        {
                            strongThis->OnCustomDataChannelDataReceived(dataView);
                        }
                    });

                strongThis->m_customChannelClosedEventRevoker = strongThis->m_customDataChannel.OnClosed(winrt::auto_revoke, [weakThis]() {
                    if (auto strongThis = weakThis.get())
                    {
                        strongThis->OnCustomDataChannelClosed();
                    }
                });
            }
        });
    }
    catch (winrt::hresult_error err)
    {
        winrt::hstring msg = err.message();
        m_errorHelper.AddError(std::wstring(L"OnDataChannelCreated failed: ") + msg.c_str());
        UpdateStatusDisplay();
    }
#endif
}


void SamplePlayerMain::InitDone()
{
    mVuforiaStarted = mController.startAR();
    if (mVuforiaStarted)
    {
        // Only reset this flag if startAR succeeded to ensure we deinit
        // Vuforia when navigating away from this page when start failed
        mVuforiaInitializing = false;

        // Change application state to running
        mAppShouldBeRunning = true;
        mLifecycleOperation = concurrency::task_from_result(true);
    }

    // Switch to UI thread to update controls
    //co_await winrt::resume_foreground(Dispatcher());
    //InitProgressRing().Visibility(Visibility::Collapsed);
    //BackButton().IsEnabled(true);
}


void SamplePlayerMain::Load(const winrt::hstring& entryPoint)
{
    /*
    InitConfig config;
    config.vbRenderBackend = VU_RENDER_VB_BACKEND_DX11;
    config.initDoneCallback = std::bind(&SamplePlayerMain::InitDone, this);

    // Bail out early if an engine instance has already been created (apps must call deinitEngine first before calling reinitialization)
    if (mEngine != nullptr)
    {
        return;
    }

    // Create engine configuration data structure
    VuEngineConfigSet* configSet = nullptr;
    REQUIRE_SUCCESS(vuEngineConfigSetCreate(&configSet));

    // Add license key to engine configuration
    auto licenseConfig = vuLicenseConfigDefault();
    licenseConfig.key = licenseKey;
    if (vuEngineConfigSetAddLicenseConfig(configSet, &licenseConfig) != VU_SUCCESS)
    {
        // Clean up before exiting
        REQUIRE_SUCCESS(vuEngineConfigSetDestroy(configSet));

        return;
    }

    // Create default render configuration (may be overwritten by platform-specific settings)
    // The default selects the platform preferred rendering backend
    auto renderConfig = vuRenderConfigDefault();
    renderConfig.vbRenderBackend = mVbRenderBackend;

        // Add platform-specific engine configuration
    VuResult platformConfigResult = VU_SUCCESS;

    // Set display orientation in platform-specific configuration
    auto vuPlatformConfig_UWP = vuPlatformUWPConfigDefault();
    vuPlatformConfig_UWP.displayOrientation = appData;

    // Add platform-specific configuration to engine configuration set
    platformConfigResult = vuEngineConfigSetAddPlatformUWPConfig(configSet, &vuPlatformConfig_UWP);

    // Check platform configuration result
    if (platformConfigResult != VU_SUCCESS)
    {
        // Clean up before exiting
        REQUIRE_SUCCESS(vuEngineConfigSetDestroy(configSet));

        //LOG("Failed to init Vuforia, could not apply platform-specific configuration");
        //mErrorMessageCallback("Vuforia failed to initialize, could not apply platform-specific configuration");
        return;
    }
    */
}



void SamplePlayerMain::Run()
{
    using Clock = std::chrono::high_resolution_clock;
    using TimePoint = Clock::time_point;
    using Duration = Clock::duration;

    Clock clock;
    TimePoint timeLastUpdate = clock.now();

    HolographicFrame prevHolographicFrame = nullptr;


    while (!m_windowClosed)
    {
        TimePoint timeCurrUpdate = clock.now();
        Duration timeSinceLastUpdate = timeCurrUpdate - timeLastUpdate;
        float deltaTimeInSeconds = std::chrono::duration<float>(timeSinceLastUpdate).count();

        //std::wostringstream woss;
        //woss << deltaTimeInSeconds;
        //m_errorHelper.AddError(woss.str());


        // If we encountered an error while creating the player context, we are going to provide
        // users with some feedback here. We have to do this after the application has launched
        // or we are going to fail at showing the dialog box.
        //"플레이어 컨텍스트를 생성하는 동안 오류가 발생했다면, 여기서 사용자에게 피드백을 제공할 것입니다. 이 작업은 애플리케이션이 시작된 "
        //"후에 수행해야 하며, 그렇지 않으면 대화 상자를 표시하는 데 실패할 것입니다."
        if (m_failedToCreatePlayerContext && !m_shownFeedbackToUser)
        {
            CoreWindow coreWindow{CoreApplication::MainView().CoreWindow().GetForCurrentThread()};

            // Window must be active or the MessageDialog will not show.
            // 창이 활성화되어 있지 않으면 메시지 대화 상자가 표시되지 않습니다
            coreWindow.Activate();

            // Dispatch call to open MessageDialog.
            // 메시지 대화 상자를 열기 위한 디스패치 호출
            coreWindow.Dispatcher().RunAsync(
                winrt::Windows::UI::Core::CoreDispatcherPriority::Normal,
                winrt::Windows::UI::Core::DispatchedHandler([]() -> winrt::fire_and_forget {
                    winrt::Windows::UI::Popups::MessageDialog failureDialog(
                        L"Failed to initialize. Please make sure that Windows Holographic is installed on your system."
                        " Windows Holographic will be installed automatically when you attach your Head-mounted Display.");

                    failureDialog.Title(L"Initialization Failure");
                    failureDialog.Commands().Append(winrt::Windows::UI::Popups::UICommand(L"Close App"));
                    failureDialog.DefaultCommandIndex(0);
                    failureDialog.CancelCommandIndex(0);

                    auto _ = co_await failureDialog.ShowAsync();

                    CoreApplication::Exit();
                }));

            m_shownFeedbackToUser = true;
        }

        if (m_windowVisible && m_deviceResources != nullptr && (m_deviceResources->GetHolographicSpace() != nullptr))
        {
            CoreWindow::GetForCurrentThread().Dispatcher().ProcessEvents(CoreProcessEventsOption::ProcessAllIfPresent);

            HolographicFrame holographicFrame = Update(deltaTimeInSeconds, prevHolographicFrame);

            Render(holographicFrame);
            prevHolographicFrame = holographicFrame;
        }
        else
        {
            CoreWindow::GetForCurrentThread().Dispatcher().ProcessEvents(CoreProcessEventsOption::ProcessOneAndAllPending);
        }

        timeLastUpdate = timeCurrUpdate;
    }
}

void SamplePlayerMain::Uninitialize()
{
#ifdef ENABLE_CUSTOM_DATA_CHANNEL_SAMPLE
    OnCustomDataChannelClosed();
#endif

    m_suspendingEventRevoker.revoke();
    m_viewActivatedRevoker.revoke();
    m_windowClosedEventRevoker.revoke();
    m_visibilityChangedEventRevoker.revoke();
    m_locatabilityChangedRevoker.revoke();

    if (m_deviceResources)
    {
        m_deviceResources->RegisterDeviceNotify(nullptr);
        m_deviceResources = nullptr;
    }
}

#pragma endregion IFrameworkView methods

#pragma region IDeviceNotify methods

void SamplePlayerMain::OnDeviceLost()
{
    m_logoImage = nullptr;

    m_statusDisplay->ReleaseDeviceDependentResources();

    // Request application restart and provide current player options to the new application instance
    std::wstringstream argsStream;
    argsStream << m_playerOptions.m_hostname.c_str() << L":" << m_playerOptions.m_port;
    if (m_playerOptions.m_listen)
    {
        argsStream << L" -listen";
    }
    if (m_playerOptions.m_showStatistics)
    {
        argsStream << L" -stats";
    }

    winrt::hstring args = argsStream.str().c_str();
    winrt::Windows::ApplicationModel::Core::CoreApplication::RequestRestartAsync(args);
}

void SamplePlayerMain::OnDeviceRestored()
{
    m_statusDisplay->CreateDeviceDependentResources();

#ifdef ENABLE_USER_COORDINATE_SYSTEM_SAMPLE
    m_simpleCubeRenderer->CreateDeviceDependentResources();
#endif

    LoadLogoImage();
}

#pragma endregion IDeviceNotify methods

void SamplePlayerMain::LoadLogoImage()
{
    m_logoImage = nullptr;

    winrt::com_ptr<ID3D11ShaderResourceView> logoView;
    winrt::check_hresult(
        DirectX::CreateDDSTextureFromFile(m_deviceResources->GetD3DDevice(), L"RemotingLogo.dds", m_logoImage.put(), logoView.put()));

    m_statusDisplay->SetImage(logoView);
}

SamplePlayerMain::PlayerOptions SamplePlayerMain::ParseActivationArgs(const IActivatedEventArgs& activationArgs)
{
    bool argsProvided = false;
    std::wstring host = L"";
    uint16_t port = 0;
    bool listen = false;
    bool showStatistics = false;

    if (activationArgs != nullptr)
    {
        ActivationKind activationKind = activationArgs.Kind();
        switch (activationKind)
        {
            case Activation::ActivationKind::Launch:
            {
                LaunchActivatedEventArgs launchArgs = activationArgs.as<LaunchActivatedEventArgs>();
                std::wstring launchArgsStr = launchArgs.Arguments().c_str();

                if (launchArgsStr.length() > 0)
                {
                    argsProvided = true;

                    std::vector<std::wstring> args;
                    std::wistringstream stream(launchArgsStr);
                    std::copy(
                        std::istream_iterator<std::wstring, wchar_t>(stream),
                        std::istream_iterator<std::wstring, wchar_t>(),
                        std::back_inserter(args));

                    for (const std::wstring& arg : args)
                    {
                        if (arg.size() == 0)
                            continue;

                        if (arg[0] == '-')
                        {
                            std::wstring param = arg.substr(1);
                            std::transform(param.begin(), param.end(), param.begin(), ::tolower);

                            if (param == L"stats")
                            {
                                showStatistics = true;
                            }

                            if (param == L"listen")
                            {
                                listen = true;
                            }

                            continue;
                        }

                        host = PlayerUtil::SplitHostnameAndPortString(arg, port);
                    }
                }
                break;
            }

            case Activation::ActivationKind::Protocol:
            {
                argsProvided = true;

                ProtocolActivatedEventArgs protocolArgs = activationArgs.as<ProtocolActivatedEventArgs>();
                auto uri = protocolArgs.Uri();
                if (uri)
                {
                    host = uri.Host();
                    port = uri.Port();

                    if (auto query = uri.QueryParsed())
                    {
                        try
                        {
                            winrt::hstring statsValue = query.GetFirstValueByName(L"stats");
                            showStatistics = true;
                        }
                        catch (...)
                        {
                        }

                        try
                        {
                            winrt::hstring statsValue = query.GetFirstValueByName(L"listen");
                            listen = true;
                        }
                        catch (...)
                        {
                        }
                    }
                }
                break;
            }
        }
    }

    PlayerOptions playerOptions;
    if (argsProvided)
    {
        // check for invalid port numbers
        if (port < 0 || port > 65535)
        {
            port = 0;
        }

        winrt::hstring hostname = host.c_str();
        if (hostname.empty())
        {
            // default to listen (as we can't connect to an unspecified host)
            hostname = L"0.0.0.0";
            listen = true;
        }

        playerOptions.m_hostname = hostname;
        playerOptions.m_port = port;
        playerOptions.m_listen = listen;
        playerOptions.m_showStatistics = showStatistics;
        playerOptions.m_ipv6 = !hostname.empty() && hostname.front() == L'[';
    }
    else
    {
        playerOptions = m_playerOptions;
    }

    return playerOptions;
}

void SamplePlayerMain::UpdateStatusDisplay()
{
    m_statusDisplay->ClearLines();

    if (m_trackingLost)
    {
        StatusDisplay::Line lines[] = {StatusDisplay::Line{L"Device Tracking Lost", StatusDisplay::Small, StatusDisplay::Yellow, 1.0f}};
        m_statusDisplay->SetLines(lines);
    }
    else
    {
        if (m_playerContext.ConnectionState() != ConnectionState::Connected)
        {
            StatusDisplay::Line lines[] = {
                StatusDisplay::Line{L"Holographic Remoting Player", StatusDisplay::LargeBold, StatusDisplay::White, 1.0f},
                StatusDisplay::Line{
                    L"This app is a companion for Holographic Remoting apps.", StatusDisplay::Small, StatusDisplay::White, 1.0f},
                StatusDisplay::Line{L"Connect from a compatible app to begin.", StatusDisplay::Small, StatusDisplay::White, 15.0f},
                StatusDisplay::Line{
                    m_playerOptions.m_listen ? L"Waiting for connection on" : L"Connecting to",
                    StatusDisplay::Small,
                    StatusDisplay::White}};
            m_statusDisplay->SetLines(lines);

            std::wostringstream addressLine;
            addressLine << (m_playerOptions.m_listen ? m_deviceIp.c_str() : m_playerOptions.m_hostname.c_str());
            if (m_playerOptions.m_port)
            {
                addressLine << L":" << m_playerOptions.m_port;
            }
            m_statusDisplay->AddLine(StatusDisplay::Line{addressLine.str(), StatusDisplay::Medium, StatusDisplay::Yellow});
            m_statusDisplay->AddLine(
                StatusDisplay::Line{L"Get help at: https://aka.ms/holographicremotinghelp", StatusDisplay::Small, StatusDisplay::White});

            if (m_playerOptions.m_showStatistics)
            {
                m_statusDisplay->AddLine(StatusDisplay::Line{L"Diagnostics Enabled", StatusDisplay::Small, StatusDisplay::Yellow});
            }
        }
        else if (m_playerContext.ConnectionState() == ConnectionState::Connected && !m_firstRemoteFrameWasBlitted)
        {
            using namespace std::chrono;

            int64_t loadingDotsCount =
                (duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count() / 250) % (s_loadingDotsMaxCount + 1);

            std::wstring dotsText;
            for (int64_t i = 0; i < loadingDotsCount; ++i)
            {
                dotsText.append(L".");
            }

            m_statusDisplay->AddLine(StatusDisplay::Line{L"", StatusDisplay::Medium, StatusDisplay::White, 7});
            m_statusDisplay->AddLine(StatusDisplay::Line{L"Receiving", StatusDisplay::Medium, StatusDisplay::White, 0.3f});
            m_statusDisplay->AddLine(StatusDisplay::Line{dotsText, StatusDisplay::Medium, StatusDisplay::White});
        }
        else
        {
            if (m_playerOptions.m_showStatistics)
            {
                std::wstring statisticsString = m_statisticsHelper.GetStatisticsString();

                StatusDisplay::Line line = {std::move(statisticsString), StatusDisplay::Medium, StatusDisplay::Yellow, 1.0f, true};
                m_statusDisplay->AddLine(line);
            }
        }
    }

    m_errorHelper.Apply(m_statusDisplay);
}

#ifdef ENABLE_CUSTOM_DATA_CHANNEL_SAMPLE
void SamplePlayerMain::OnCustomDataChannelDataReceived(winrt::array_view<const uint8_t> dataView)
{
    std::vector<uint8_t> answer;
    bool alwaysSend = false;

    const uint8_t packetType = (dataView.size() > 0) ? dataView[0] : 1;
    switch (packetType)
    {
        case 1: // simple echo ping
            answer.resize(1);
            answer[0] = 1;
            break;

        default:
            return; // no answer to unknown packets
    }

    std::lock_guard customDataChannelLockGuard(m_customDataChannelLock);
    if (m_customDataChannel)
    {
        // Get send queue size. The send queue size returns the size of data, that has not been send yet, in bytes. A big number can
        // indicate that more data is being queued for sending than is actually getting sent. If possible skip sending data in this
        // case, to help the queue getting smaller again.

        // 전송 큐 크기를 가져옵니다. 전송 큐 크기는 아직 전송되지 않은 데이터의 크기를 바이트 단위로 반환합니다. 큰 숫자는 실제로 전송되는
        // 것보다 더 많은 데이터가 전송을 위해 큐에 들어가고 있음을 나타낼 수 있습니다. 가능하다면 이 경우 데이터 전송을 건너뛰어 큐 크기가
        // 다시 줄어들도록 도와주세요.

        uint32_t sendQueueSize = m_customDataChannel.SendQueueSize();

        // Only send the packet if the send queue is smaller than 1MiB
        if (alwaysSend || sendQueueSize < 1 * 1024 * 1024)
        {
            try
            {
              //  m_errorHelper.AddError(answer.data());
                m_customDataChannel.SendData(winrt::array_view<const uint8_t>{answer.data(), static_cast<uint32_t>(answer.size())}, true);
            }
            catch (...)
            {
                // SendData might throw if channel is closed, but we did not get or process the async closed event yet.
            }
        }
    }
}

void SamplePlayerMain::OnCustomDataChannelClosed()
{
    std::lock_guard customDataChannelLockGuard(m_customDataChannelLock);
    if (m_customDataChannel)
    {
        m_customChannelDataReceivedEventRevoker.revoke();
        m_customChannelClosedEventRevoker.revoke();
        m_customDataChannel = nullptr;
    }
}
#endif

void SamplePlayerMain::OnConnected()
{
    m_errorHelper.ClearErrors();
    UpdateStatusDisplay();
}

void SamplePlayerMain::OnDisconnected(ConnectionFailureReason reason)
{
    m_errorHelper.ClearErrors();
    bool error = m_errorHelper.ProcessOnDisconnect(reason);

    m_firstRemoteFrameWasBlitted = false;

    UpdateStatusDisplay();

    if (error)
    {
        ConnectOrListenAfter(1s);
        return;
    }

    // Reconnect quickly if not an error
    ConnectOrListenAfter(200ms);
}

void SamplePlayerMain::OnRequestRenderTargetSize(
    winrt::Windows::Foundation::Size requestedSize, winrt::Windows::Foundation::Size providedSize)
{
    // Store the new remote render target size
    // Note: We'll use the provided size as remote side content is going to be resampled/distorted anyway,
    // so there is no point in resolving this information into a smaller backbuffer on the player side.
    // 새로운 원격 렌더 타깃 크기를 저장합니다.
    // 참고: 제공된 크기를 사용할 것입니다. 원격 측 컨텐츠는 어차피 재샘플링되거나 왜곡될 예정이므로,
    // 플레이어 측에서 이 정보를 더 작은 백버퍼로 해석할 필요가 없습니다.
    std::lock_guard lock{m_renderTargetSizeChangeMutex};
    m_needRenderTargetSizeChange = true;
    m_newRenderTargetSize = providedSize;
}

#pragma region Spatial locator event handlers

void SamplePlayerMain::OnLocatabilityChanged(const SpatialLocator& sender, const winrt::Windows::Foundation::IInspectable& args)
{
    bool wasTrackingLost = m_trackingLost;

    switch (sender.Locatability())
    {
        case SpatialLocatability::PositionalTrackingActive:
            m_trackingLost = false;
            break;

        default:
            m_trackingLost = true;
            break;
    }

    if (m_statusDisplay && m_trackingLost != wasTrackingLost)
    {
        UpdateStatusDisplay();
    }
}

#pragma endregion Spatial locator event handlers

#pragma region Application lifecycle event handlers

void SamplePlayerMain::OnViewActivated(const CoreApplicationView& sender, const IActivatedEventArgs& activationArgs)
{
    PlayerOptions playerOptionsNew = ParseActivationArgs(activationArgs);

    // Prevent diagnostics to be turned off everytime the app went to background.
    if (activationArgs.PreviousExecutionState() != ApplicationExecutionState::NotRunning)
    {
        if (!playerOptionsNew.m_showStatistics)
        {
            playerOptionsNew.m_showStatistics = m_playerOptions.m_showStatistics;
        }
    }

    m_playerOptions = playerOptionsNew;

    if (m_playerContext.ConnectionState() == ConnectionState::Disconnected)
    {
        // Try to connect to or listen on the provided hostname/port
        ConnectOrListen();
    }
    else
    {
        UpdateStatusDisplay();
    }

    sender.CoreWindow().Activate();
}

void SamplePlayerMain::OnSuspending(const winrt::Windows::Foundation::IInspectable& sender, const SuspendingEventArgs& args)
{
    m_deviceResources->Trim();

    // Disconnect when app is about to suspend.
    if (m_playerContext.ConnectionState() != ConnectionState::Disconnected)
    {
        m_playerContext.Disconnect();
    }
}

#pragma endregion Application lifecycle event handlers

#pragma region Window event handlers

void SamplePlayerMain::OnVisibilityChanged(const CoreWindow& sender, const VisibilityChangedEventArgs& args)
{
    m_windowVisible = args.Visible();
}

void SamplePlayerMain::OnWindowClosed(const CoreWindow& sender, const CoreWindowEventArgs& args)
{
    m_windowClosed = true;
}

#pragma endregion Window event handlers

int __stdcall wWinMain(HINSTANCE, HINSTANCE, PWSTR, int)
{
    winrt::init_apartment();
    winrt::com_ptr<SamplePlayerMain> main = winrt::make_self<SamplePlayerMain>();
    CoreApplication::Run(*main);
    return 0;
}
