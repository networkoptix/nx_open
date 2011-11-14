#include "ui_layout_item.h"
#include <cassert>
#include <utils/common/scene_utility.h>
#include <core/resource/resource.h>
#include <core/resourcemanagment/resource_pool.h>
#include "ui_layout.h"
#include "ui_display.h"

QnUiLayoutItem::QnUiLayoutItem(const QString &resourceUniqueId, QObject *parent): 
    QObject(parent), 
    m_resourceUniqueId(resourceUniqueId),
    m_layout(NULL),
    m_flags(Pinned),
    m_rotation(0.0)
{}

QnUiLayoutItem::~QnUiLayoutItem() {
    ensureRemoved();
}

void QnUiLayoutItem::ensureRemoved() {
    if(m_layout != NULL)
        m_layout->removeItem(this);
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

QnUiDisplay *QnUiLayoutItem::createDisplay(QObject *parent) {
    QnResourcePtr resource = qnResPool->getResourceByUniqId(m_resourceUniqueId);
    if(resource.isNull())
        return NULL;
    return new QnUiDisplay(resource, parent);
}