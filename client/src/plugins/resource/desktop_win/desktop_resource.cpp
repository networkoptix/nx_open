#include "desktop_resource.h"

#ifdef Q_OS_WIN

#include "plugins/resource/desktop_win/desktop_data_provider.h"
#include "ui/screen_recording/video_recorder_settings.h"
#include "core/resource/media_server_resource.h"
#include "plugins/resource/desktop_camera/desktop_camera_connection.h"

namespace {
    const QnUuid desktopResourceUuid(lit("{B3B2235F-D279-4d28-9012-00DE1002A61D}"));
}

//static QnDesktopResource* instance = 0;

QnDesktopResource::QnDesktopResource(QGLWidget* mainWindow): QnAbstractArchiveResource() 
{
    m_mainWidget = mainWindow;
    addFlags(Qn::local_live_cam | Qn::desktop_camera);

    const QString name = lit("Desktop");
    setName(name);
    setUrl(name);
    m_desktopDataProvider = 0;
    setId(desktopResourceUuid); // only one desktop resource is allowed)
  //  setDisabled(true);
    //Q_ASSERT_X(instance == 0, "Only one instance of desktop camera now allowed!", Q_FUNC_INFO);
    //instance = this;
}

QnDesktopResource::~QnDesktopResource()
{
    delete m_desktopDataProvider;
    //instance = 0;
}

QString QnDesktopResource::toString() const {
    return getUniqueId();
}

QnAbstractStreamDataProvider* QnDesktopResource::createDataProviderInternal(Qn::ConnectionRole /*role*/) 
{
    SCOPED_MUTEX_LOCK( lock, &m_dpMutex);

    createSharedDataProvider();
    if (m_desktopDataProvider == 0)
        return 0;

    QnDesktopDataProviderWrapper* p = new QnDesktopDataProviderWrapper(toSharedPointer(), m_desktopDataProvider);
    m_desktopDataProvider->addDataProcessor(p);
    return p;
}

void QnDesktopResource::createSharedDataProvider()
{
    if (m_desktopDataProvider) 
    {
        if (m_desktopDataProvider->readyToStop())
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
#else
#endif
}

bool QnDesktopResource::isRendererSlow() const
{
    QnVideoRecorderSettings recorderSettings;
    Qn::CaptureMode captureMode = recorderSettings.captureMode();
    return captureMode == Qn::FullScreenMode;
}

void QnDesktopResource::addConnection(const QnMediaServerResourcePtr &server)
{
    if (m_connectionPool.contains(server->getId()))
        return;
    QnDesktopCameraConnectionPtr connection = QnDesktopCameraConnectionPtr(new QnDesktopCameraConnection(this, server));
    m_connectionPool[server->getId()] = connection;
    connection->start();
}

void QnDesktopResource::removeConnection(const QnMediaServerResourcePtr &server)
{
    m_connectionPool.remove(server->getId());
}

static QSharedPointer<QnEmptyResourceAudioLayout> emptyAudioLayout( new QnEmptyResourceAudioLayout() );
QnConstResourceAudioLayoutPtr QnDesktopResource::getAudioLayout(const QnAbstractStreamDataProvider* /*dataProvider*/) const
{
    if (!m_desktopDataProvider)
        return emptyAudioLayout;
    return m_desktopDataProvider->getAudioLayout();
}

QnUuid QnDesktopResource::getDesktopResourceUuid() {
    return desktopResourceUuid;
}


#endif
