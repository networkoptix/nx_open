#include "desktop_resource.h"

#ifdef Q_OS_WIN

#include "device_plugins/desktop_win/desktop_data_provider.h"
#include "ui/screen_recording/video_recorder_settings.h"
#include "ui/style/skin.h"
#include "core/resource/media_server_resource.h"
#include "device_plugins/desktop_camera/desktop_camera_connection.h"

namespace {
    const QUuid desktopResourceUuid(lit("{B3B2235F-D279-4d28-9012-00DE1002A61D}"));
}

//static QnDesktopResource* instance = 0;

QnDesktopResource::QnDesktopResource(QGLWidget* mainWindow): QnAbstractArchiveResource() 
{
    m_mainWidget = mainWindow;
    addFlags(local_live_cam);

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

QnAbstractStreamDataProvider* QnDesktopResource::createDataProviderInternal(ConnectionRole /*role*/) 
{
    QMutexLocker lock(&m_dpMutex);

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

    bool captureCursor = recorderSettings.captureCursor();
    QSize encodingSize = QnVideoRecorderSettings::resolutionToSize(recorderSettings.resolution());
    float encodingQuality = QnVideoRecorderSettings::qualityToNumeric(recorderSettings.decoderQuality());

    m_desktopDataProvider = new QnDesktopDataProvider(toSharedPointer(),
        audioDevice.isNull() ? 0 : &audioDevice,
        secondAudioDevice.isNull() ? 0 : &secondAudioDevice,
        captureCursor,
        encodingSize,
        encodingQuality,
        m_mainWidget,
        QPixmap());
#else
#endif
}

void QnDesktopResource::addConnection(QnMediaServerResourcePtr mServer)
{
    if (m_connectionPool.contains(mServer->getId()))
        return;
    QnDesktopCameraConnectionPtr connection = QnDesktopCameraConnectionPtr(new QnDesktopCameraConnection(this, mServer));
    m_connectionPool[mServer->getId()] = connection;
    connection->start();
}

void QnDesktopResource::removeConnection(QnMediaServerResourcePtr mServer)
{
    m_connectionPool.remove(mServer->getId());
}

static std::shared_ptr<QnEmptyResourceAudioLayout> emptyAudioLayout( new QnEmptyResourceAudioLayout() );
QnConstResourceAudioLayoutPtr QnDesktopResource::getAudioLayout(const QnAbstractStreamDataProvider* /*dataProvider*/)
{
    if (!m_desktopDataProvider)
        return emptyAudioLayout;
    return m_desktopDataProvider->getAudioLayout();
}

QUuid QnDesktopResource::getDesktopResourceUuid() {
    return desktopResourceUuid;
}


#endif
