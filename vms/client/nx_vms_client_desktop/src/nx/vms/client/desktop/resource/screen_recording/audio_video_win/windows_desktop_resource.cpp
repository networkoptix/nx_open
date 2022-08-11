// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "windows_desktop_resource.h"

#include <QtWidgets/QOpenGLWidget>

#include <core/resource/media_server_resource.h>
#include <nx/vms/client/core/resource/screen_recording/desktop_camera_connection.h>
#include <nx/vms/client/desktop/application_context.h>

#include "../video_recorder_settings.h"
#include "desktop_data_provider.h"

namespace nx::vms::client::desktop {

WindowsDesktopResource::WindowsDesktopResource(QOpenGLWidget* mainWindow):
    m_mainWidget(mainWindow)
{
    addFlags(Qn::local_live_cam | Qn::desktop_camera);

    const QString name = "Desktop";
    setName(name);
    setUrl(name);
    setIdUnsafe(core::DesktopResource::getDesktopResourceUuid()); // only one desktop resource is allowed)
}

WindowsDesktopResource::~WindowsDesktopResource()
{
    delete m_desktopDataProvider;
}

bool WindowsDesktopResource::hasVideo(const QnAbstractStreamDataProvider* /*dataProvider*/) const
{
    return true;
}

QnAbstractStreamDataProvider* WindowsDesktopResource::createDataProviderInternal()
{
    NX_MUTEX_LOCKER lock( &m_dpMutex );

    createSharedDataProvider();
    if (!m_desktopDataProvider)
        return nullptr;

    auto p = new DesktopDataProviderWrapper(toSharedPointer(), m_desktopDataProvider);
    m_desktopDataProvider->addDataProcessor(p);
    return p;
}

void WindowsDesktopResource::createSharedDataProvider()
{
    if (m_desktopDataProvider)
    {
        if (m_desktopDataProvider->readyToStop())
            delete m_desktopDataProvider; // stop and destroy old instance
        else
            return; // already exists
    }

    appContext()->voiceSpectrumAnalyzer()->reset();

    VideoRecorderSettings recorderSettings;

    core::AudioDeviceInfo audioDevice = recorderSettings.primaryAudioDevice();
    core::AudioDeviceInfo secondAudioDevice;
    if (recorderSettings.secondaryAudioDevice().fullName() != audioDevice.fullName())
        secondAudioDevice = recorderSettings.secondaryAudioDevice();
    if (core::AudioDeviceInfo::availableDevices(QAudio::AudioInput).isEmpty()) {
        audioDevice = core::AudioDeviceInfo(); // no audio devices
        secondAudioDevice = core::AudioDeviceInfo();
    }
    int screen = VideoRecorderSettings::screenToAdapter(recorderSettings.screen());
    bool captureCursor = recorderSettings.captureCursor();
    QSize encodingSize = VideoRecorderSettings::resolutionToSize(recorderSettings.resolution());
    float encodingQuality = VideoRecorderSettings::qualityToNumeric(recorderSettings.decoderQuality());
    Qn::CaptureMode captureMode = recorderSettings.captureMode();

    m_desktopDataProvider = new DesktopDataProvider(toSharedPointer(),
        screen,
        audioDevice.isNull() ? 0 : &audioDevice,
        secondAudioDevice.isNull() ? 0 : &secondAudioDevice,
        captureMode,
        captureCursor,
        encodingSize,
        encodingQuality,
        m_mainWidget,
        QPixmap());
}

bool WindowsDesktopResource::isRendererSlow() const
{
    VideoRecorderSettings recorderSettings;
    Qn::CaptureMode captureMode = recorderSettings.captureMode();
    return captureMode == Qn::FullScreenMode;
}

static AudioLayoutConstPtr emptyAudioLayout(new AudioLayout());

AudioLayoutConstPtr WindowsDesktopResource::getAudioLayout(const QnAbstractStreamDataProvider* /*dataProvider*/) const
{
    if (m_desktopDataProvider && m_desktopDataProvider->getAudioLayout())
        return m_desktopDataProvider->getAudioLayout();
    return emptyAudioLayout;
}

QnAbstractStreamDataProvider* WindowsDesktopResource::createDataProvider(
    const QnResourcePtr& resource,
    Qn::ConnectionRole role)
{
    const auto desktopResource = resource.dynamicCast<WindowsDesktopResource>();
    NX_ASSERT(desktopResource);
    if (!desktopResource)
        return nullptr;

    return desktopResource->createDataProviderInternal();
}

} // namespace nx::vms::client::desktop
