#include "resource_display.h"
#include <cassert>
#include <core/dataprovider/media_streamdataprovider.h>
#include <core/resource/resource_media_layout.h>
#include <core/resource/media_resource.h>
#include <plugins/resources/archive/abstract_archive_stream_reader.h>
#include <camera/camdisplay.h>
#include <camera/camera.h>
#include <camera/abstractrenderer.h>
#include <utils/common/warnings.h>
#include <utils/common/counter.h>

detail::QnRendererGuard::~QnRendererGuard() {
    delete m_renderer;
}

QnResourceDisplay::QnResourceDisplay(const QnResourcePtr &resource, QObject *parent):
    QObject(parent),
    QnResourceConsumer(resource),
    m_dataProvider(NULL),
    m_mediaProvider(NULL),
    m_archiveReader(NULL),
    m_camera(NULL),
    m_started(false)
{
    assert(!resource.isNull());

    m_mediaResource = resource.dynamicCast<QnMediaResource>();

    m_dataProvider = resource->createDataProvider(QnResource::Role_Default);


    if(m_dataProvider != NULL) {
        m_archiveReader = dynamic_cast<QnAbstractArchiveReader *>(m_dataProvider);
#if 0
        // motion detection test
        QRegion region;
        region += QRect(0,0, 5,4);
        m_archiveReader->setMotionRegion(region);
#endif


        m_mediaProvider = dynamic_cast<QnAbstractMediaStreamDataProvider *>(m_dataProvider);
        if(m_mediaProvider != NULL) {
            m_camera = new CLVideoCamera(m_mediaResource, NULL, false, m_mediaProvider);
            
            QnCounter *counter = new QnCounter(2);
            connect(m_camera->getCamCamDisplay(),   SIGNAL(finished()),         counter,        SLOT(increment()));
            connect(m_mediaProvider,                SIGNAL(finished()),         counter,        SLOT(increment()));
            connect(counter,                        SIGNAL(targetReached()),    counter,        SLOT(deleteLater()));
            connect(counter,                        SIGNAL(targetReached()),    m_camera,       SLOT(deleteLater()));
        } else {
            m_camera = NULL;

            connect(this, SIGNAL(destroyed()), m_dataProvider, SLOT(stop()));
            connect(m_dataProvider, SIGNAL(finished()), m_dataProvider, SLOT(deleteLater()));
        }
    }
}

QnResourceDisplay::~QnResourceDisplay() {
    beforeDisconnectFromResource();
    disconnectFromResource();
}

void QnResourceDisplay::cleanUp(CLLongRunnable *runnable) const {
    if(runnable == NULL)
        return;

    if(m_started) {
        runnable->pleaseStop();
    } else {
        runnable->deleteLater();
    }
}

CLCamDisplay *QnResourceDisplay::camDisplay() const {
    if(m_camera == NULL)
        return NULL;

    return m_camera->getCamCamDisplay();
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
        m_mediaProvider->removeDataProcessor(m_camera->getCamCamDisplay());

    cleanUp(m_dataProvider);
    if(m_camera != NULL)
        m_camera->beforeStopDisplay();

    foreach(detail::QnRendererGuard *guard, m_guards)
        guard->renderer()->beforeDestroy();

    if(!m_started)
        foreach(detail::QnRendererGuard *guard, m_guards)
            delete guard;

    m_mediaResource.clear();
    m_dataProvider = NULL;
    m_mediaProvider = NULL;
    m_archiveReader = NULL;
    m_camera = NULL;

    QnResourceConsumer::disconnectFromResource();
}

const QnVideoResourceLayout *QnResourceDisplay::videoLayout() const {
    if(m_mediaProvider == NULL)
        return NULL;

    if(m_mediaResource == NULL)
        return NULL;

    return m_mediaResource->getVideoLayout(m_mediaProvider);
}

qint64 QnResourceDisplay::lengthUSec() const {
    return m_archiveReader == NULL ? -1 : m_archiveReader->lengthMksec();
}

qint64 QnResourceDisplay::currentTimeUSec() const {
    return m_archiveReader == NULL ? -1 : m_archiveReader->currentTime();
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
    //    m_camera->getCamCamDisplay()->playAudio(m_playing);
}

void QnResourceDisplay::pause() {
    if(m_archiveReader == NULL)
        return;

    m_archiveReader->pause();
    m_archiveReader->setSingleShotMode(true);
    m_archiveReader->pauseDataProcessors();

    //m_camera->getCamCamDisplay()->pauseAudio();
}

bool QnResourceDisplay::isPaused() {

    if(m_archiveReader == NULL)
        return false;


    return m_archiveReader->isMediaPaused();
}

void QnResourceDisplay::addRenderer(CLAbstractRenderer *renderer) {
    if(m_camera == NULL) {
        delete renderer;
        return;
    }

    int channelCount = videoLayout()->numberOfChannels();
    for(int i = 0; i < channelCount; i++)
        m_camera->getCamCamDisplay()->addVideoChannel(i, renderer, true);

    m_guards.push_back(new detail::QnRendererGuard(renderer));
    connect(m_camera->getCamCamDisplay(), SIGNAL(finished()), m_guards.back(), SLOT(deleteLater()));
}

