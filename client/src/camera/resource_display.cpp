#include "resource_display.h"
#include <cassert>
#include <core/dataprovider/media_streamdataprovider.h>
#include <core/resource/resource_media_layout.h>
#include <core/resource/media_resource.h>
#include <core/resource/media_server_resource.h>
#include <plugins/resource/archive/abstract_archive_stream_reader.h>
#include <camera/cam_display.h>
#include <camera/client_video_camera.h>
#include <camera/abstract_renderer.h>
#include <utils/common/warnings.h>
#include <utils/common/counter.h>
#include <utils/common/util.h>

QnResourceDisplay::QnResourceDisplay(const QnResourcePtr &resource, QObject *parent):
    QObject(parent),
    QnResourceConsumer(resource),
    m_dataProvider(NULL),
    m_mediaProvider(NULL),
    m_archiveReader(NULL),
    m_camera(NULL),
    m_started(false),
    m_counter(0)
{
    assert(!resource.isNull());

    m_mediaResource = resource.dynamicCast<QnMediaResource>();

    m_dataProvider = resource->createDataProvider(Qn::CR_Default);

    if(m_dataProvider != NULL) {
        m_archiveReader = dynamic_cast<QnAbstractArchiveReader *>(m_dataProvider);
        m_mediaProvider = dynamic_cast<QnAbstractMediaStreamDataProvider *>(m_dataProvider);

        if(m_mediaProvider != NULL) {
            /* Camera will free media provider in its destructor. */
            m_camera = new QnClientVideoCamera(m_mediaResource, m_mediaProvider);

            connect(this,                           SIGNAL(destroyed()),    m_camera,       SLOT(beforeStopDisplay()));

            m_counter = new QnCounter(1);
            connect(m_camera->getCamDisplay(),      SIGNAL(finished()),     m_counter,      SLOT(decrement()));
            if (m_mediaProvider->hasThread()) {
                connect(m_camera->getStreamreader(),    SIGNAL(finished()),     m_counter,      SLOT(decrement()));
                m_counter->increment();
            }

            connect(m_counter,                      SIGNAL(reachedZero()),  m_counter,      SLOT(deleteLater()));
            connect(m_counter,                      SIGNAL(reachedZero()),  m_camera,       SLOT(deleteLater()));
        } else {
            m_camera = NULL;

            connect(this,           SIGNAL(destroyed()),    m_dataProvider, SLOT(pleaseStop()));
            connect(m_dataProvider, SIGNAL(finished()),     m_dataProvider, SLOT(deleteLater()));
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
    if(m_dataProvider == NULL)
        return;

    static_cast<QnResourceConsumer *>(m_dataProvider)->beforeDisconnectFromResource();
}

void QnResourceDisplay::disconnectFromResource() {
    if(m_dataProvider == NULL)
        return; 

    if(m_mediaProvider != NULL)
        m_mediaProvider->removeDataProcessor(m_camera->getCamDisplay());

    cleanUp(m_dataProvider);
    if(m_camera != NULL)
        m_camera->beforeStopDisplay();

    m_mediaResource.clear();
    m_dataProvider = NULL;
    m_mediaProvider = NULL;
    m_archiveReader = NULL;
    m_camera = NULL;

    QnResourceConsumer::disconnectFromResource();
}

QnConstResourceVideoLayoutPtr QnResourceDisplay::videoLayout() const {
    if(m_mediaProvider == NULL)
        return QnConstResourceVideoLayoutPtr();

    if(m_mediaResource == NULL)
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

void QnResourceDisplay::setCurrentTimeUSec(qint64 usec) const {
    if(m_archiveReader == NULL) {
        qnWarning("Resource '%1' does not support changing current time.", resource()->getUniqueId());
        return;
    }

    m_archiveReader->previousFrame(usec);
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
    if(m_archiveReader == NULL)
        return;

    m_archiveReader->pauseMedia();
}

bool QnResourceDisplay::isPaused() {
    if(m_archiveReader == NULL)
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
