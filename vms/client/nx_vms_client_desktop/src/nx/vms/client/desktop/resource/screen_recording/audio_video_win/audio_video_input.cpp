// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "audio_video_input.h"

#if defined(Q_OS_WIN)
    #include <d3d9.h>
#endif

#include <QtGui/QGuiApplication>
#include <QtGui/QScreen>
#include <QtMultimedia/QMediaDevices>
#include <QtOpenGLWidgets/QOpenGLWidget>

#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/resource/screen_recording/audio_video_win/desktop_data_provider.h>
#include <nx/vms/client/desktop/resource/screen_recording/desktop_data_provider_wrapper.h>
#include <nx/vms/client/desktop/settings/screen_recording_settings.h>

using namespace nx::vms::client::desktop::screen_recording;

namespace {

int screenToAdapter(int screen)
{
    #if defined(Q_OS_WIN)
        IDirect3D9* pD3D;
        if ((pD3D = Direct3DCreate9(D3D_SDK_VERSION)) == nullptr)
            return 0;

        const auto screens = QGuiApplication::screens();
        if (!NX_ASSERT(screen >= 0 && screen < screens.size()))
            return 0;

        QRect rect = screens.at(screen)->geometry();
        MONITORINFO monitorInfo;
        memset(&monitorInfo, 0, sizeof(monitorInfo));
        monitorInfo.cbSize = sizeof(monitorInfo);

        int result = 0;
        for (int i = 0; i < screens.size(); ++i)
        {
            if (!GetMonitorInfo(pD3D->GetAdapterMonitor(i), &monitorInfo))
                break;

            if (monitorInfo.rcMonitor.left == rect.left()
                && monitorInfo.rcMonitor.top == rect.top())
            {
                result = i;
                break;
            }
        }
        pD3D->Release();
        return result;
    #else
        return screen;
    #endif
}

QSize resolutionToSize(Resolution resolution)
{
    switch (resolution)
    {
        case Resolution::native:
            return QSize(0, 0);
        case Resolution::quarterNative:
            return QSize(-2, -2);
        case Resolution::_1920x1080:
            return QSize(1920, 1080);
        case Resolution::_1280x720:
            return QSize(1280, 720);
        case Resolution::_640x480:
            return QSize(640, 480);
        case Resolution::_320x240:
            return QSize(320, 240);
        default:
            NX_ASSERT(false,
                "Invalid resolution value '%1', treating as native resolution.",
                (int) resolution);
            return QSize(0, 0);
    }
}

float qualityToNumeric(Quality quality)
{
    switch (quality)
    {
        case Quality::best:
            return 1.0;
        case Quality::balanced:
            return 0.75;
        case Quality::performance:
            return 0.5;
        default:
            NX_ASSERT(
                false, "Invalid quality value '%1', treating as best quality.", (int) quality);
            return 1.0;
    }
}

} // namespace

namespace nx::vms::client::desktop {

AudioVideoInput::AudioVideoInput(QOpenGLWidget* mainWidget):
    m_mainWidget(mainWidget)
{
}

std::unique_ptr<QnAbstractStreamDataProvider> AudioVideoInput::createAudioInputProvider() const
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    createSharedDataProvider();
    if (!m_provider)
        return nullptr;

    return std::make_unique<DesktopDataProviderWrapper>(m_provider.get());
}

void AudioVideoInput::createSharedDataProvider() const
{
    if (m_provider)
    {
        if (m_provider->readyToStop())
            m_provider.reset(); //< Stop and destroy the old instance.
        else
            return; //< The instance already exists.
    }

    appContext()->voiceSpectrumAnalyzer()->reset();
    core::AudioDeviceInfo audioDevice = screenRecordingSettings()->primaryAudioDevice();
    core::AudioDeviceInfo secondAudioDevice;
    if (screenRecordingSettings()->secondaryAudioDevice().fullName() != audioDevice.fullName())
        secondAudioDevice = screenRecordingSettings()->secondaryAudioDevice();
    if (QMediaDevices::audioInputs().isEmpty())
    {
        audioDevice = core::AudioDeviceInfo(); //< No audio devices.
        secondAudioDevice = core::AudioDeviceInfo();
    }
    int screen = screenToAdapter(screenRecordingSettings()->screen());
    bool captureCursor = screenRecordingSettings()->captureCursor();
    QSize encodingSize = resolutionToSize(screenRecordingSettings()->resolution());
    float encodingQuality = qualityToNumeric(screenRecordingSettings()->quality());
    CaptureMode captureMode = screenRecordingSettings()->captureMode();

    QString videoCodecName = "mpeg4";
    m_provider = std::make_unique<DesktopDataProvider>(screen,
        audioDevice.isNull() ? 0 : &audioDevice,
        secondAudioDevice.isNull() ? 0 : &secondAudioDevice,
        captureMode,
        captureCursor,
        encodingSize,
        encodingQuality,
        videoCodecName,
        m_mainWidget,
        /*logo*/ QPixmap());
}

} // namespace nx::vms::client::desktop
