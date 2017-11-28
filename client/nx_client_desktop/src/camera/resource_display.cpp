#include "resource_display.h"
#include <cassert>
#include <nx/streaming/abstract_media_stream_data_provider.h>
#include <core/resource/resource_media_layout.h>
#include <core/resource/media_resource.h>
#include <core/resource/media_server_resource.h>
#include <nx/streaming/abstract_archive_stream_reader.h>
#include <camera/cam_display.h>
#include <camera/client_video_camera.h>
#include <camera/abstract_renderer.h>
#include <utils/common/warnings.h>
#include <nx/utils/counter.h>
#include <utils/common/util.h>

QnResourceDisplay::QnResourceDisplay(const QnResourcePtr &resource, QObject *parent):
    base_type(parent),
    QnResourceConsumer(resource),
    m_dataProvider(),
    m_mediaProvider(),
    m_archiveReader(),
    m_camera(),
    m_started(false),
    m_counter()
{
    NX_ASSERT(resource);

    m_mediaResource = resource.dynamicCast<QnMediaResource>();

    m_dataProvider = resource->createDataProvider(Qn::CR_Default);

    if (m_dataProvider)
    {
        m_archiveReader = dynamic_cast<QnAbstractArchiveStreamReader *>(m_dataProvider.data());
        m_mediaProvider = dynamic_cast<QnAbstractMediaStreamDataProvider *>(m_dataProvider.data());

        if (m_mediaProvider != NULL)
        {
            /* Camera will free media provider in its destructor. */
            m_camera = new QnClientVideoCamera(m_mediaResource, m_mediaProvider);

            connect(this, &QObject::destroyed, m_camera, &QnClientVideoCamera::beforeStopDisplay);

            m_counter = new nx::utils::Counter(1);
            connect(m_camera->getCamDisplay(), &QnCamDisplay::finished, m_counter, &nx::utils::Counter::decrement);
            if (m_mediaProvider->hasThread())
            {
                connect(m_camera->getStreamreader(), &QnAbstractMediaStreamDataProvider::finished,
                    m_counter, &nx::utils::Counter::decrement);
                m_counter->increment();
            }

            connect(m_counter, &nx::utils::Counter::reachedZero, m_counter, &QObject::deleteLater);
            connect(m_counter, &nx::utils::Counter::reachedZero, m_camera, &QObject::deleteLater);
        }
        else
        {
            m_camera = nullptr;

            connect(this, &QObject::destroyed, m_dataProvider,
                &QnAbstractStreamDataProvider::pleaseStop);
            connect(m_dataProvider, &QnAbstractStreamDataProvider::finished,
                m_dataProvider, &QObject::deleteLater);
        }

    }
}

QnResourceDisplay::~QnResourceDisplay()
{
    if (m_camera && !m_camera->isDisplayStarted())
    {
        if (m_counter)
            m_counter->deleteLater();
        m_camera->deleteLater();
    }

    beforeDisconnectFromResource();
    disconnectFromResource();
}

void QnResourceDisplay::beforeDestroy()
{
}

const QnResourcePtr &QnResourceDisplay::resource() const {
    return getResource();
}

const QnMediaResourcePtr &QnResourceDisplay::mediaResource() const {
    return m_mediaResource;
}

QnAbstractStreamDataProvider* QnResourceDisplay::dataProvider() const
{
    return m_dataProvider.data();
}

QnAbstractMediaStreamDataProvider* QnResourceDisplay::mediaProvider() const
{
    return m_mediaProvider.data();
}

QnAbstractArchiveStreamReader* QnResourceDisplay::archiveReader() const
{
    return m_archiveReader.data();
}

QnClientVideoCamera* QnResourceDisplay::camera() const
{
    return m_camera.data();
}

void QnResourceDisplay::cleanUp(QnLongRunnable *runnable) const {
    if(runnable == NULL)
        return;

#if 0
    if(m_started) {
        runnable->pleaseStop();
    } else {
        runnable->deleteLater();
    }
#endif
    runnable->pleaseStop();
}

QnCamDisplay *QnResourceDisplay::camDisplay() const {
    if(m_camera == NULL)
        return NULL;

    return m_camera->getCamDisplay();
}

void QnResourceDisplay::beforeDisconnectFromResource() {
    if (!m_dataProvider)
        return;

    static_cast<QnResourceConsumer *>(m_dataProvider.data())->beforeDisconnectFromResource();
}

void QnResourceDisplay::disconnectFromResource() {
    if (!m_dataProvider)
        return;

    if (m_mediaProvider && m_camera)
        m_mediaProvider->removeDataProcessor(m_camera->getCamDisplay());

    cleanUp(m_dataProvider);
    if (m_camera)
        m_camera->beforeStopDisplay();

    m_mediaResource.clear();
    m_dataProvider.clear();
    m_mediaProvider.clear();
    m_archiveReader.clear();
    m_camera.clear();

    QnResourceConsumer::disconnectFromResource();
}

QnConstResourceVideoLayoutPtr QnResourceDisplay::videoLayout() const {
    if (!m_mediaProvider)
        return QnConstResourceVideoLayoutPtr();

    if (!m_mediaResource)
        return QnConstResourceVideoLayoutPtr();

    return m_mediaResource->getVideoLayout(m_mediaProvider);
}

qint64 QnResourceDisplay::lengthUSec() const {
    return m_archiveReader == NULL ? -1 : m_archiveReader->lengthUsec();
}

qint64 QnResourceDisplay::currentTimeUSec() const {
    if (!m_camera)
        return -1;
    else if (m_camera->getCamDisplay()->isRealTimeSource())
        return DATETIME_NOW;
    else
        return m_camera->getCamDisplay()->getCurrentTime();
    //return m_archiveReader == NULL ? -1 : m_archiveReader->currentTime();
}

void QnResourceDisplay::start() {
    m_started = true;

    if(m_camera != NULL)
        m_camera->startDisplay();
}

void QnResourceDisplay::play() {
    if(m_archiveReader == NULL)
        return;

    m_archiveReader->resumeMedia();

    //if (m_graphicsWidget->isSelected() || !m_playing)
    //    m_camera->getCamDisplay()->playAudio(m_playing);
}

void QnResourceDisplay::pause() {
    if (!m_archiveReader)
        return;

    m_archiveReader->pauseMedia();
}

bool QnResourceDisplay::isPaused() {
    if (!m_archiveReader)
        return false;

    return m_archiveReader->isMediaPaused();
}

bool QnResourceDisplay::isStillImage() const {
    return m_resource->flags() & Qn::still_image;
}

void QnResourceDisplay::addRenderer(QnAbstractRenderer *renderer) {
    if (m_camera)
        m_camera->getCamDisplay()->addVideoRenderer(videoLayout()->channelCount(), renderer, true);
}

void QnResourceDisplay::removeRenderer(QnAbstractRenderer *renderer) {
    int channelCount = videoLayout()->channelCount();
    if (m_camera) {
        for(int i = 0; i < channelCount; i++)
            m_camera->getCamDisplay()->removeVideoRenderer(renderer);
    }
}

void QnResourceDisplay::addMetadataConsumer(
    const nx::media::AbstractMetadataConsumerPtr& metadataConsumer)
{
    if (m_camera)
        m_camera->getCamDisplay()->addMetadataConsumer(metadataConsumer);
}

void QnResourceDisplay::removeMetadataConsumer(
    const nx::media::AbstractMetadataConsumerPtr& metadataConsumer)
{
    if (m_camera)
        m_camera->getCamDisplay()->removeMetadataConsumer(metadataConsumer);
}
