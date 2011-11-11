#include "ui_layout_item.h"
#include <cassert>
#include <core/dataprovider/media_streamdataprovider.h>
#include <core/resource/resource_media_layout.h>
#include <plugins/resources/archive/abstract_archive_stream_reader.h>
#include <camera/camdisplay.h>
#include <utils/common/scene_utility.h>
#include <utils/common/warnings.h>
#include "ui_layout.h"

QnUiLayoutItem::QnUiLayoutItem(const QnResourcePtr &resource, QObject *parent): 
    QObject(parent), 
    QnResourceConsumer(resource),
    m_layout(NULL),
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

QnUiLayoutItem::~QnUiLayoutItem() {
    ensureRemoved();
}

void QnUiLayoutItem::ensureRemoved() {
    if(m_layout != NULL)
        m_layout->removeItem(this);
}

QnResource *QnUiLayoutItem::resource() const {
    return getResource().data();
}

QnMediaResource *QnUiLayoutItem::mediaResource() const {
    return dynamic_cast<QnMediaResource *>(resource()); // TODO: remove dynamic_cast.
}

void QnUiLayoutItem::beforeDisconnectFromResource() {
    static_cast<QnResourceConsumer *>(m_dataProvider)->beforeDisconnectFromResource();
}

void QnUiLayoutItem::disconnectFromResource() {
    static_cast<QnResourceConsumer *>(m_dataProvider)->disconnectFromResource();

    QnResourceConsumer::disconnectFromResource();
}

bool QnUiLayoutItem::setGeometry(const QRect &geometry) {
    if(m_layout != NULL)
        return m_layout->moveItem(this, geometry);

    setGeometryInternal(geometry);
    return true;
}

void QnUiLayoutItem::setGeometryInternal(const QRect &geometry) {
    if(m_geometry == geometry)
        return;

    QRect oldGeometry = m_geometry;
    m_geometry = geometry;

    emit geometryChanged(oldGeometry, m_geometry);
}

bool QnUiLayoutItem::setGeometryDelta(const QRectF &geometryDelta) {
    if(isPinned())
        return false;

    if(qFuzzyCompare(m_geometryDelta, geometryDelta))
        return true;

    QRectF oldGeometryDelta = m_geometryDelta;
    m_geometryDelta = geometryDelta;

    emit geometryDeltaChanged(oldGeometryDelta, m_geometryDelta);
    return true;
}

bool QnUiLayoutItem::setFlag(ItemFlag flag, bool value) {
    if(checkFlag(flag) == value)
        return true;

    if(m_layout != NULL) {
        if(flag == Pinned && value)
            return m_layout->pinItem(this, m_geometry);

        if(flag == Pinned && !value)
            return m_layout->unpinItem(this);
    }
    
    setFlagInternal(flag, value);
    return true;
}

void QnUiLayoutItem::setFlagInternal(ItemFlag flag, bool value) {
    if(checkFlag(flag) == value)
        return;

    if(flag == Pinned && value)
        setGeometryDelta(QRectF()); /* Pinned items cannot have non-zero geometry delta. */

    ItemFlags oldFlags = m_flags;
    m_flags = value ? (m_flags | flag) : (m_flags & ~flag);

    emit flagsChanged(oldFlags, m_flags);
}

void QnUiLayoutItem::setRotation(qreal rotation) {
    if(qFuzzyCompare(m_rotation, rotation))
        return;

    qreal oldRotation = m_rotation;
    m_rotation = rotation;

    emit rotationChanged(oldRotation, m_rotation);
}

qint64 QnUiLayoutItem::lengthUSec() const {
    return m_archiveProvider == NULL ? -1 : m_archiveProvider->lengthMksec();
}

qint64 QnUiLayoutItem::currentTimeUSec() const {
    return m_archiveProvider == NULL ? -1 : m_archiveProvider->currentTime();
}

void QnUiLayoutItem::setCurrentTimeUSec(qint64 usec) const {
    if(m_archiveProvider == NULL) {
        qnWarning("Resource '%1' does not support changing current time.", resource()->getUniqueId());
        return;
    }

    m_archiveProvider->previousFrame(usec);
}

void QnUiLayoutItem::play() {
    if(m_archiveProvider == NULL)
        return;

    m_archiveProvider->resume();
    m_archiveProvider->setSingleShotMode(false);
    m_archiveProvider->resumeDataProcessors();

    //if (m_graphicsWidget->isSelected() || !m_playing)
    //    m_camera->getCamCamDisplay()->playAudio(m_playing);
}

void QnUiLayoutItem::pause() {
    if(m_archiveProvider == NULL)
        return;

    m_archiveProvider->pause();
    m_archiveProvider->setSingleShotMode(true);
    m_archiveProvider->pauseDataProcessors();
    
    //m_camera->getCamCamDisplay()->pauseAudio();
}
