// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "desktop_resource.h"

#include <QtWidgets/QOpenGLWidget>

#include <core/resource/media_server_resource.h>
#include <nx/vms/client/desktop/application_context.h>
#include <plugins/resource/desktop_camera/desktop_camera_connection.h>
#include <plugins/resource/desktop_win/desktop_data_provider.h>
#include <ui/screen_recording/video_recorder_settings.h>

using namespace nx::vms::client::desktop;

QnWinDesktopResource::QnWinDesktopResource(QOpenGLWidget* mainWindow):
    m_mainWidget(mainWindow)
{
    addFlags(Qn::local_live_cam | Qn::desktop_camera);

    const QString name = lit("Desktop");
    setName(name);
    setUrl(name);
    setIdUnsafe(QnDesktopResource::getDesktopResourceUuid()); // only one desktop resource is allowed)
}

QnWinDesktopResource::~QnWinDesktopResource()
{
    delete m_desktopDataProvider;
}

bool QnWinDesktopResource::hasVideo(const QnAbstractStreamDataProvider* /*dataProvider*/) const
{
    return true;
}

QnAbstractStreamDataProvider* QnWinDesktopResource::createDataProviderInternal()
{
    NX_MUTEX_LOCKER lock( &m_dpMutex );

    createSharedDataProvider();
    if (!m_desktopDataProvider)
        return nullptr;

    QnDesktopDataProviderWrapper* p = new QnDesktopDataProviderWrapper(toSharedPointer(), m_desktopDataProvider);
    m_desktopDataProvider->addDataProcessor(p);
    return p;
}

void QnWinDesktopResource::createSharedDataProvider()
{
    if (m_desktopDataProvider)
    {
        if (m_desktopDataProvider->readyToStop())
            delete m_desktopDataProvider; // stop and destroy old instance
        else
            return; // already exists
    }

    appContext()->voiceSpectrumAnalyzer()->reset();

    QnVideoRecorderSettings recorderSettings;

    QnAudioDeviceInfo audioDevice = recorderSettings.primaryAudioDevice();
    QnAudioDeviceInfo secondAudioDevice;
    if (recorderSettings.secondaryAudioDevice().fullName() != audioDevice.fullName())
        secondAudioDevice = recorderSettings.secondaryAudioDevice();
    if (QnAudioDeviceInfo::availableDevices(QAudio::AudioInput).isEmpty()) {
        audioDevice = QnAudioDeviceInfo(); // no audio devices
        secondAudioDevice = QnAudioDeviceInfo();
    }
    int screen = QnVideoRecorderSettings::screenToAdapter(recorderSettings.screen());
    bool captureCursor = recorderSettings.captureCursor();
    QSize encodingSize = QnVideoRecorderSettings::resolutionToSize(recorderSettings.resolution());
    float encodingQuality = QnVideoRecorderSettings::qualityToNumeric(recorderSettings.decoderQuality());
    Qn::CaptureMode captureMode = recorderSettings.captureMode();

    m_desktopDataProvider = new QnDesktopDataProvider(toSharedPointer(),
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

bool QnWinDesktopResource::isRendererSlow() const
{
    QnVideoRecorderSettings recorderSettings;
    Qn::CaptureMode captureMode = recorderSettings.captureMode();
    return captureMode == Qn::FullScreenMode;
}

static AudioLayoutConstPtr emptyAudioLayout(new AudioLayout());

AudioLayoutConstPtr QnWinDesktopResource::getAudioLayout(const QnAbstractStreamDataProvider* /*dataProvider*/) const
{
    if (m_desktopDataProvider && m_desktopDataProvider->getAudioLayout())
        return m_desktopDataProvider->getAudioLayout();
    return emptyAudioLayout;
}

QnAbstractStreamDataProvider* QnWinDesktopResource::createDataProvider(
    const QnResourcePtr& resource,
    Qn::ConnectionRole role)
{
    const auto desktopResource = resource.dynamicCast<QnWinDesktopResource>();
    NX_ASSERT(desktopResource);
    if (!desktopResource)
        return nullptr;

    return desktopResource->createDataProviderInternal();
}
