#include "desktop_resource.h"
#include "device_plugins/desktop_win/desktop_data_provider.h"
#include "ui/screen_recording/video_recorder_settings.h"
#include "ui/style/skin.h"

QnDesktopResource::QnDesktopResource(int index, QGLWidget* mainWindow): QnAbstractArchiveResource() 
{
    m_mainWidget = mainWindow;
    addFlags(local_live_cam);

    const QString name = QLatin1String("Desktop") + QString::number(index + 1);
    setName(name);
    setUrl(name);
    m_desktopDataProvider = 0;
}

QnDesktopResource::~QnDesktopResource()
{
    delete m_desktopDataProvider;
}


QString QnDesktopResource::toString() const {
    return getUniqueId();
}

QnAbstractStreamDataProvider* QnDesktopResource::createDataProviderInternal(ConnectionRole /*role*/) 
{
    QMutexLocker lock(&m_dpMutex);

    createSharedDataProvider();
    if (m_desktopDataProvider == 0)
        return 0;

    QnDesktopDataProviderWrapper* p = new QnDesktopDataProviderWrapper(toSharedPointer());
    m_desktopDataProvider->addDataProcessor(p);
    return p;
}

void QnDesktopResource::beforeStartDataProvider(QnDesktopDataProviderWrapper*)
{
    QMutexLocker lock(&m_dpMutex);
    if (m_desktopDataProvider)
        m_desktopDataProvider->start();
}

void QnDesktopResource::beforeDestroyDataProvider(QnDesktopDataProviderWrapper* dataProviderWrapper)
{
    QMutexLocker lock(&m_dpMutex);

    if (m_desktopDataProvider) {
        m_desktopDataProvider->removeDataProcessor(dataProviderWrapper);
        if (m_desktopDataProvider->processorsCount() == 0) 
            m_desktopDataProvider->pleaseStop();
    }
}

void QnDesktopResource::createSharedDataProvider()
{
    if (m_desktopDataProvider) 
    {
        if (m_desktopDataProvider->needToStop())
            delete m_desktopDataProvider; // stop and destroy old instance
        else
            return; // already exists
    }

#ifdef Q_OS_WIN

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

    QPixmap logo;
#if defined(CL_TRIAL_MODE) || defined(CL_FORCE_LOGO)
    //QString logoName = QString("logo_") + QString::number(encodingSize.width()) + QString("_") + QString::number(encodingSize.height()) + QString(".png");
    QString logoName = QLatin1String("logo_1920_1080.png");
    logo = qnSkin->pixmap(logoName); // hint: comment this line to remove logo
#endif

    m_desktopDataProvider = new QnDesktopDataProvider(toSharedPointer(),
        screen,
        audioDevice.isNull() ? 0 : &audioDevice,
        secondAudioDevice.isNull() ? 0 : &secondAudioDevice,
        captureMode,
        captureCursor,
        encodingSize,
        encodingQuality,
        m_mainWidget,
        logo);
#else
#endif
}

bool QnDesktopResource::isRendererSlow() const
{
    QnVideoRecorderSettings recorderSettings;
    Qn::CaptureMode captureMode = recorderSettings.captureMode();
    return captureMode == Qn::FullScreenMode;
}
