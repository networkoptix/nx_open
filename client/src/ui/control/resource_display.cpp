#include "resource_display.h"
#include <cassert>
#include <core/dataprovider/media_streamdataprovider.h>
#include <core/resource/resource_media_layout.h>
#include <plugins/resources/archive/abstract_archive_stream_reader.h>
#include <camera/camdisplay.h>
#include <utils/common/warnings.h>


QnResourceDisplay::QnResourceDisplay(const QnResourcePtr &resource, QObject *parent):
    QObject(parent),
    QnResourceConsumer(resource)
{
    assert(!resource.isNull());

    // TODO: leaking resources here.

    m_dataProvider = resource->createDataProvider(QnResource::Role_Default);
    m_archiveProvider = dynamic_cast<QnAbstractArchiveReader *>(m_dataProvider);

    m_mediaProvider = dynamic_cast<QnAbstractMediaStreamDataProvider *>(m_dataProvider);
    if(m_mediaProvider != NULL) {
        m_camDisplay.reset(new CLCamDisplay(false));

        m_mediaProvider->addDataProcessor(m_camDisplay.data());
    }
}

QnResource *QnResourceDisplay::resource() const {
    return getResource().data();
}

QnMediaResource *QnResourceDisplay::mediaResource() const {
    return dynamic_cast<QnMediaResource *>(resource()); // TODO: remove dynamic_cast.
}

void QnResourceDisplay::beforeDisconnectFromResource() {
    static_cast<QnResourceConsumer *>(m_dataProvider)->beforeDisconnectFromResource();
}

void QnResourceDisplay::disconnectFromResource() {
    static_cast<QnResourceConsumer *>(m_dataProvider)->disconnectFromResource();

    QnResourceConsumer::disconnectFromResource();
}

void QnResourceDisplay::start() {
    m_dataProvider->start();

    if(!m_camDisplay.isNull())
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

    m_archiveProvider->resume();
    m_archiveProvider->setSingleShotMode(false);
    m_archiveProvider->resumeDataProcessors();

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
