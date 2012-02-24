#include "workbench_item.h"
#include <limits>
#include <ui/common/scene_utility.h>
#include <core/resource/layout_resource.h>
#include "workbench_layout.h"
#include "core/resourcemanagment/resource_pool.h"

QnWorkbenchItem::QnWorkbenchItem(const QString &resourceUid, const QUuid &uuid, QObject *parent):
    QObject(parent),
    m_resourceUid(resourceUid),
    m_uuid(uuid),
    m_layout(NULL),
    m_flags(0),
    m_rotation(0.0)
{}

QnWorkbenchItem::~QnWorkbenchItem() {
    if (m_layout)
        m_layout->removeItem(this);
}

bool QnWorkbenchItem::load(const QnLayoutItemData &itemData) {
    if(itemData.uuid != uuid())
        qnWarning("Loading item '%1' from data with different uuid (%2 != %3).", resourceUid(), itemData.uuid.toString(), uuid().toString());

#ifdef _DEBUG
    QnId localId = qnResPool->getResourceByUniqId(resourceUid())->getId();
    if(itemData.resource.id != localId && itemData.resource.path != m_resourceUid)
        qnWarning("Loading item '%1' from a data with different ids (%2 != %3 and %4 != %5).", resourceUid(), localId.toString(), itemData.resource.id.toString(), itemData.resource.path, m_resourceUid);
#endif

    bool result = true;

    /* Note that the order of these calls is important. */
    result &= setFlag(Pinned, false);
    result &= setCombinedGeometry(itemData.combinedGeometry);
    setRotation(itemData.rotation);
    result &= setFlags(static_cast<ItemFlags>(itemData.flags));

    return result;
}

void QnWorkbenchItem::save(QnLayoutItemData &itemData) const {
    if(itemData.uuid != uuid())
        qnWarning("Saving item '%1' to a data with different uuid (%2 != %3).", resourceUid(), itemData.uuid, uuid());

#ifdef _DEBUG
    QnId localId = qnResPool->getResourceByUniqId(resourceUid())->getId();
    if(itemData.resource.id != localId && itemData.resource.path != m_resourceUid)
        qnWarning("Saving item '%1' to a data with different ids (%2 != %3 and %4 != %5).", resourceUid(), localId.toString(), itemData.resource.id.toString(), itemData.resource.path, m_resourceUid);
#endif

    itemData.flags = flags();
    itemData.rotation = rotation();
    itemData.combinedGeometry = combinedGeometry();
}

bool QnWorkbenchItem::setGeometry(const QRect &geometry) {
    if(m_layout != NULL)
        return m_layout->moveItem(this, geometry);

    setGeometryInternal(geometry);
    return true;
}

void QnWorkbenchItem::setGeometryInternal(const QRect &geometry) {
    if(m_geometry == geometry)
        return;

    m_geometry = geometry;

    emit geometryChanged();
}

bool QnWorkbenchItem::setGeometryDelta(const QRectF &geometryDelta) {
    if(isPinned())
        return false;

    if(qFuzzyCompare(m_geometryDelta, geometryDelta))
        return true;

    m_geometryDelta = geometryDelta;

    emit geometryDeltaChanged();
    return true;
}

bool QnWorkbenchItem::setCombinedGeometry(const QRectF &combinedGeometry) {
    QRect geometry = combinedGeometry.toRect(); /* Conversion uses rounding, so all differences are in [-0.5, 0.5]. */
    QRectF geometryDelta = QRectF(
        combinedGeometry.topLeft() - geometry.topLeft(),
        combinedGeometry.size() - geometry.size()
    );

    if(isPinned() && (!qFuzzyIsNull(geometryDelta.topLeft()) || !qFuzzyIsNull(geometryDelta.size())))
        return false; /* Cannot set geometry delta for pinned items. */

    if(!setGeometry(geometry))
        return false;

    if(!isPinned())
        setGeometryDelta(geometryDelta);

    return true;
}

QRectF QnWorkbenchItem::combinedGeometry() const {
    return QRectF(
        m_geometry.left() + m_geometryDelta.left(),
        m_geometry.top() + m_geometryDelta.top(),
        m_geometry.width() + m_geometryDelta.width(),
        m_geometry.height() + m_geometryDelta.height()
    );
}

bool QnWorkbenchItem::setFlag(ItemFlag flag, bool value) {
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

bool QnWorkbenchItem::setFlags(ItemFlags flags) {
    if(m_flags == flags)
        return true;

    bool result = true;
    if((m_flags ^ flags) & Pinned)
        result &= setFlag(Pinned, flags & Pinned);
    if((m_flags ^ flags) & PendingGeometryAdjustment)
        result &= setFlag(PendingGeometryAdjustment, flags & PendingGeometryAdjustment);


    return result;
}

void QnWorkbenchItem::setFlagInternal(ItemFlag flag, bool value) {
    if(checkFlag(flag) == value)
        return;

    if(flag == Pinned && value)
        setGeometryDelta(QRectF()); /* Pinned items cannot have non-zero geometry delta. */

    m_flags = value ? (m_flags | flag) : (m_flags & ~flag);

    emit flagChanged(flag, value);
}

void QnWorkbenchItem::setRotation(qreal rotation) {
    if(qFuzzyCompare(m_rotation, rotation))
        return;

    m_rotation = rotation;

    emit rotationChanged();
}

void QnWorkbenchItem::adjustGeometry() {
    /* Invalidate geometry. */
    QPoint topLeft = m_geometry.topLeft();
    setGeometry(QRect(topLeft + QPoint(1, 1), topLeft - QPoint(1, 1)));
    setGeometryDelta(QRectF());

    /* Set geometry adjustment flag. */
    setFlag(PendingGeometryAdjustment, true);
}

void QnWorkbenchItem::adjustGeometry(const QPointF &desiredPosition) {
    /* Update combined geometry to reflect desired position. */
    QRectF combinedGeometry = this->combinedGeometry();
    QPointF delta = desiredPosition - combinedGeometry.center();
    combinedGeometry = QRectF(
        combinedGeometry.topLeft() + delta,
        combinedGeometry.bottomRight() + delta
    );
    setCombinedGeometry(combinedGeometry);

    /* Set geometry adjustment flag. */
    setFlag(PendingGeometryAdjustment, true);
}
