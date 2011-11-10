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
    m_reader(NULL),
    m_archiveReader(NULL)
{
    assert(!resource.isNull());

    m_reader = resource->createDataProvider(QnResource::Role_Default);
    m_archiveReader = dynamic_cast<QnAbstractArchiveReader *>(m_reader);

    QnAbstractMediaStreamDataProvider *mediaReader = dynamic_cast<QnAbstractMediaStreamDataProvider *>(m_reader);
    if(mediaReader != NULL) {
        m_camDisplay.reset(new CLCamDisplay(false));

        mediaReader->addDataProcessor(m_camDisplay.data());
    }

    m_reader->start();
    m_camDisplay->start();
}

QnDisplayEntity::~QnDisplayEntity() {
    ensureRemoved();
}

void QnDisplayEntity::ensureRemoved() {
    if(m_model != NULL)
        m_model->removeEntity(this);
}

void QnDisplayEntity::beforeDisconnectFromResource() {
    static_cast<QnResourceConsumer *>(m_reader)->beforeDisconnectFromResource();
}

void QnDisplayEntity::disconnectFromResource() {
    static_cast<QnResourceConsumer *>(m_reader)->disconnectFromResource();

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
    return m_archiveReader == NULL ? -1 : m_archiveReader->lengthMksec();
}

qint64 QnDisplayEntity::currentTimeUSec() const {
    return m_archiveReader == NULL ? -1 : m_archiveReader->currentTime();
}

void QnDisplayEntity::setCurrentTimeUSec(qint64 usec) const {
    if(m_archiveReader == NULL) {
        qnWarning("Resource '%1' does not support changing current time.", resource()->getUniqueId());
        return;
    }

    m_archiveReader->previousFrame(usec);
}

void QnDisplayEntity::play() {
    if(m_archiveReader == NULL)
        return;

    m_archiveReader->resume();
    m_archiveReader->setSingleShotMode(false);
    m_archiveReader->resumeDataProcessors();

    //if (m_graphicsWidget->isSelected() || !m_playing)
    //    m_camera->getCamCamDisplay()->playAudio(m_playing);
}

void QnDisplayEntity::pause() {
    if(m_archiveReader == NULL)
        return;

    m_archiveReader->pause();
    m_archiveReader->setSingleShotMode(true);
    m_archiveReader->pauseDataProcessors();
    
    //m_camera->getCamCamDisplay()->pauseAudio();
}
