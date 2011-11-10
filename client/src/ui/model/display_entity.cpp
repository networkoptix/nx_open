#include "display_entity.h"
#include <cassert>
#include <core/dataprovider/media_streamdataprovider.h>
#include <core/resource/resource_media_layout.h>
#include <plugins/resources/archive/abstract_archive_stream_reader.h>
#include <camera/camdisplay.h>
#include <utils/common/scene_utility.h>
#include <utils/common/warnings.h>
#include "display_model.h"

QnDisplayEntity::QnDisplayEntity(const QnResourcePtr &resource, QObject *parent): 
    QObject(parent), 
    QnResourceConsumer(resource),
    m_model(NULL),
    m_flags(Pinned),
    m_rotation(0.0),
    m_dataProvider(NULL),
    m_mediaProvider(NULL),
    m_archiveProvider(NULL)
{
    assert(!resource.isNull());

    m_dataProvider = resource->createDataProvider(QnResource::Role_Default);
    m_archiveProvider = dynamic_cast<QnAbstractArchiveReader *>(m_dataProvider);

    m_mediaProvider = dynamic_cast<QnAbstractMediaStreamDataProvider *>(m_dataProvider);
    if(m_mediaProvider != NULL) {
        m_camDisplay.reset(new CLCamDisplay(false));

        m_mediaProvider->addDataProcessor(m_camDisplay.data());
    }

    m_dataProvider->start();
    m_camDisplay->start();
}

QnDisplayEntity::~QnDisplayEntity() {
    ensureRemoved();
}

void QnDisplayEntity::ensureRemoved() {
    if(m_model != NULL)
        m_model->removeEntity(this);
}

QnResource *QnDisplayEntity::resource() const {
    return getResource().data();
}

QnMediaResource *QnDisplayEntity::mediaResource() const {
    return dynamic_cast<QnMediaResource *>(resource()); // TODO: remove dynamic_cast.
}

void QnDisplayEntity::beforeDisconnectFromResource() {
    static_cast<QnResourceConsumer *>(m_dataProvider)->beforeDisconnectFromResource();
}

void QnDisplayEntity::disconnectFromResource() {
    static_cast<QnResourceConsumer *>(m_dataProvider)->disconnectFromResource();

    QnResourceConsumer::disconnectFromResource();
}

bool QnDisplayEntity::setGeometry(const QRect &geometry) {
    if(m_model != NULL)
        return m_model->moveEntity(this, geometry);

    setGeometryInternal(geometry);
    return true;
}

void QnDisplayEntity::setGeometryInternal(const QRect &geometry) {
    if(m_geometry == geometry)
        return;

    QRect oldGeometry = m_geometry;
    m_geometry = geometry;

    emit geometryChanged(oldGeometry, m_geometry);
}

bool QnDisplayEntity::setGeometryDelta(const QRectF &geometryDelta) {
    if(isPinned())
        return false;

    if(qFuzzyCompare(m_geometryDelta, geometryDelta))
        return true;

    QRectF oldGeometryDelta = m_geometryDelta;
    m_geometryDelta = geometryDelta;

    emit geometryDeltaChanged(oldGeometryDelta, m_geometryDelta);
    return true;
}

bool QnDisplayEntity::setFlag(EntityFlag flag, bool value) {
    if(checkFlag(flag) == value)
        return true;

    if(m_model != NULL) {
        if(flag == Pinned && value)
            return m_model->pinEntity(this, m_geometry);

        if(flag == Pinned && !value)
            return m_model->unpinEntity(this);
    }
    
    setFlagInternal(flag, value);
    return true;
}

void QnDisplayEntity::setFlagInternal(EntityFlag flag, bool value) {
    if(checkFlag(flag) == value)
        return;

    if(flag == Pinned && value)
        setGeometryDelta(QRectF()); /* Pinned entities cannot have non-zero geometry delta. */

    EntityFlags oldFlags = m_flags;
    m_flags = value ? (m_flags | flag) : (m_flags & ~flag);

    emit flagsChanged(oldFlags, m_flags);
}

void QnDisplayEntity::setRotation(qreal rotation) {
    if(qFuzzyCompare(m_rotation, rotation))
        return;

    qreal oldRotation = m_rotation;
    m_rotation = rotation;

    emit rotationChanged(oldRotation, m_rotation);
}

qint64 QnDisplayEntity::lengthUSec() const {
    return m_archiveProvider == NULL ? -1 : m_archiveProvider->lengthMksec();
}

qint64 QnDisplayEntity::currentTimeUSec() const {
    return m_archiveProvider == NULL ? -1 : m_archiveProvider->currentTime();
}

void QnDisplayEntity::setCurrentTimeUSec(qint64 usec) const {
    if(m_archiveProvider == NULL) {
        qnWarning("Resource '%1' does not support changing current time.", resource()->getUniqueId());
        return;
    }

    m_archiveProvider->previousFrame(usec);
}

void QnDisplayEntity::play() {
    if(m_archiveProvider == NULL)
        return;

    m_archiveProvider->resume();
    m_archiveProvider->setSingleShotMode(false);
    m_archiveProvider->resumeDataProcessors();

    //if (m_graphicsWidget->isSelected() || !m_playing)
    //    m_camera->getCamCamDisplay()->playAudio(m_playing);
}

void QnDisplayEntity::pause() {
    if(m_archiveProvider == NULL)
        return;

    m_archiveProvider->pause();
    m_archiveProvider->setSingleShotMode(true);
    m_archiveProvider->pauseDataProcessors();
    
    //m_camera->getCamCamDisplay()->pauseAudio();
}
