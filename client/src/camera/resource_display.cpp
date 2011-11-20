#include "resource_display.h"
#include <cassert>
#include <core/dataprovider/media_streamdataprovider.h>
#include <core/resource/resource_media_layout.h>
#include <core/resource/media_resource.h>
#include <plugins/resources/archive/abstract_archive_stream_reader.h>
#include <camera/camdisplay.h>
#include <camera/abstractrenderer.h>
#include <utils/common/warnings.h>

detail::QnRendererGuard::~QnRendererGuard() {
    delete m_renderer;
}

QnResourceDisplay::QnResourceDisplay(const QnResourcePtr &resource, QObject *parent):
    QObject(parent),
    QnResourceConsumer(resource),
    m_dataProvider(NULL),
    m_mediaProvider(NULL),
    m_archiveProvider(NULL),
    m_camDisplay(NULL),
    m_started(false)
{
    assert(!resource.isNull());

    m_mediaResource = dynamic_cast<QnMediaResource *>(resource.data());

    m_dataProvider = resource->createDataProvider(QnResource::Role_Default);
    if(m_dataProvider != NULL) {
        connect(m_dataProvider, SIGNAL(finished()), m_dataProvider, SLOT(deleteLater()));

        m_archiveProvider = dynamic_cast<QnAbstractArchiveReader *>(m_dataProvider);

        m_mediaProvider = dynamic_cast<QnAbstractMediaStreamDataProvider *>(m_dataProvider);
        if(m_mediaProvider != NULL) {
            m_camDisplay = new CLCamDisplay(false);
            connect(m_camDisplay, SIGNAL(finished()), m_camDisplay, SLOT(deleteLater()));

            m_mediaProvider->addDataProcessor(m_camDisplay);
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

QnResource *QnResourceDisplay::resource() const {
    return getResource().data();
}

QnMediaResource *QnResourceDisplay::mediaResource() const {
    return resource() == NULL ? NULL : m_mediaResource;
}

void QnResourceDisplay::beforeDisconnectFromResource() {
    if(m_dataProvider == NULL)
        return;

    static_cast<QnResourceConsumer *>(m_dataProvider)->beforeDisconnectFromResource();
}

void QnResourceDisplay::disconnectFromResource() {
    if(m_dataProvider == NULL)
        return; 

    static_cast<QnResourceConsumer *>(m_dataProvider)->disconnectFromResource();

    if(m_mediaProvider != NULL)
        m_mediaProvider->removeDataProcessor(m_camDisplay);

    cleanUp(m_dataProvider);
    cleanUp(m_camDisplay);

    foreach(detail::QnRendererGuard *guard, m_guards)
        guard->renderer()->beforeDestroy();

    if(!m_started)
        foreach(detail::QnRendererGuard *guard, m_guards)
            delete guard;

    m_mediaResource = NULL;

    m_dataProvider = NULL;
    m_mediaProvider = NULL;
    m_archiveProvider = NULL;

    m_camDisplay = NULL;

    QnResourceConsumer::disconnectFromResource();
}

const QnVideoResourceLayout *QnResourceDisplay::videoLayout() const {
    if(m_mediaProvider == NULL)
        return NULL;

    if(m_mediaResource == NULL)
        return NULL;

    return m_mediaResource->getVideoLayout(m_mediaProvider);
}

void QnResourceDisplay::start() {
    m_started = true;

    if(m_dataProvider != NULL)
        m_dataProvider->start();

    if(m_camDisplay != NULL)
        m_camDisplay->start();
}

qint64 QnResourceDisplay::lengthUSec() const {
    return m_archiveProvider == NULL ? -1 : m_archiveProvider->lengthMksec();
}

qint64 QnResourceDisplay::currentTimeUSec() const {
    return m_archiveProvider == NULL ? -1 : m_archiveProvider->currentTime();
}

void QnResourceDisplay::setCurrentTimeUSec(qint64 usec) const {
    if(m_archiveProvider == NULL) {
        qnWarning("Resource '%1' does not support changing current time.", resource()->getUniqueId());
        return;
    }

    m_archiveProvider->previousFrame(usec);
}

void QnResourceDisplay::play() {
    if(m_archiveProvider == NULL)
        return;

    m_archiveProvider->resumeMedia();

    //if (m_graphicsWidget->isSelected() || !m_playing)
    //    m_camera->getCamCamDisplay()->playAudio(m_playing);
}

void QnResourceDisplay::pause() {
    if(m_archiveProvider == NULL)
        return;

    m_archiveProvider->pause();
    m_archiveProvider->setSingleShotMode(true);
    m_archiveProvider->pauseDataProcessors();

    //m_camera->getCamCamDisplay()->pauseAudio();
}

void QnResourceDisplay::addRenderer(CLAbstractRenderer *renderer) {
    if(m_camDisplay == NULL) {
        delete renderer;
        return;
    }

    int channelCount = videoLayout()->numberOfChannels();
    for(int i = 0; i < channelCount; i++)
        m_camDisplay->addVideoChannel(i, renderer, true);

    m_guards.push_back(new detail::QnRendererGuard(renderer));
    connect(m_camDisplay, SIGNAL(finished()), m_guards.back(), SLOT(deleteLater()));
}

