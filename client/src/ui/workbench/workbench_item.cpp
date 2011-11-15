#include "resource_item_model.h"
#include <cassert>
#include <utils/common/scene_utility.h>
#include <core/resource/resource.h>
#include <core/resourcemanagment/resource_pool.h>
#include <ui/control/resource_display.h>
#include "layout_model.h"

QnLayoutItemModel::QnLayoutItemModel(const QString &resourceUniqueId, QObject *parent): 
    QObject(parent), 
    m_resourceUniqueId(resourceUniqueId),
    m_layout(NULL),
    m_flags(Pinned),
    m_rotation(0.0)
{}

QnLayoutItemModel::~QnLayoutItemModel() {
    ensureRemoved();
}

void QnLayoutItemModel::ensureRemoved() {
    if(m_layout != NULL)
        m_layout->removeItem(this);
}

bool QnLayoutItemModel::setGeometry(const QRect &geometry) {
    if(m_layout != NULL)
        return m_layout->moveItem(this, geometry);

    setGeometryInternal(geometry);
    return true;
}

void QnLayoutItemModel::setGeometryInternal(const QRect &geometry) {
    if(m_geometry == geometry)
        return;

    QRect oldGeometry = m_geometry;
    m_geometry = geometry;

    emit geometryChanged(oldGeometry, m_geometry);
}

bool QnLayoutItemModel::setGeometryDelta(const QRectF &geometryDelta) {
    if(isPinned())
        return false;

    if(qFuzzyCompare(m_geometryDelta, geometryDelta))
        return true;

    QRectF oldGeometryDelta = m_geometryDelta;
    m_geometryDelta = geometryDelta;

    emit geometryDeltaChanged(oldGeometryDelta, m_geometryDelta);
    return true;
}

bool QnLayoutItemModel::setFlag(ItemFlag flag, bool value) {
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

void QnLayoutItemModel::setFlagInternal(ItemFlag flag, bool value) {
    if(checkFlag(flag) == value)
        return;

    if(flag == Pinned && value)
        setGeometryDelta(QRectF()); /* Pinned items cannot have non-zero geometry delta. */

    ItemFlags oldFlags = m_flags;
    m_flags = value ? (m_flags | flag) : (m_flags & ~flag);

    emit flagsChanged(oldFlags, m_flags);
}

void QnLayoutItemModel::setRotation(qreal rotation) {
    if(qFuzzyCompare(m_rotation, rotation))
        return;

    qreal oldRotation = m_rotation;
    m_rotation = rotation;

    emit rotationChanged(oldRotation, m_rotation);
}

QnResourceDisplay *QnLayoutItemModel::createDisplay(QObject *parent) {
    QnResourcePtr resource = qnResPool->getResourceByUniqId(m_resourceUniqueId);
    if(resource.isNull())
        return NULL;
    return new QnResourceDisplay(resource, parent);
}