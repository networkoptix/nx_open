#include "workbench_item.h"
#include <limits>
#include <ui/common/geometry.h>
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

QnWorkbenchItem::QnWorkbenchItem(const QnLayoutItemData &data, QObject *parent):
    QObject(parent),
    m_resourceUid(data.resource.path),
    m_uuid(data.uuid),
    m_layout(NULL),
    m_flags(0),
    m_rotation(0.0)
{
    if(m_resourceUid.isEmpty()) {
        qnWarning("Creating a workbench item from item data with invalid unique id.");
        // TODO: fix layout item data conventions.

        QnResourcePtr resource = qnResPool->getResourceById(data.resource.id);
        if(resource)
            m_resourceUid = resource->getUniqueId();
    }

    setFlags(static_cast<Qn::ItemFlags>(data.flags));
    setRotation(data.rotation);
    setCombinedGeometry(data.combinedGeometry);
}

QnWorkbenchItem::~QnWorkbenchItem() {
    if (m_layout)
        m_layout->removeItem(this);
}

QnLayoutItemData QnWorkbenchItem::data() const {
    QnResourcePtr resource = qnResPool->getResourceByUniqId(m_resourceUid);

    QnLayoutItemData data;

    data.uuid = m_uuid;
    data.resource.path = m_resourceUid;
    data.resource.id = resource ? resource->getId() : QnId();
    data.flags = flags();
    data.rotation = rotation();
    data.combinedGeometry = combinedGeometry();

    return data;
}

bool QnWorkbenchItem::update(const QnLayoutItemData &data) {
    if(data.uuid != uuid())
        qnWarning("Updating item '%1' from data with different uuid (%2 != %3).", resourceUid(), data.uuid.toString(), uuid().toString());

#ifdef _DEBUG
    QnResourcePtr resource = qnResPool->getResourceByUniqId(resourceUid());
    QnId localId = resource ? resource->getId() : QnId();
    if(data.resource.id != localId && data.resource.path != m_resourceUid)
        qnWarning("Updating item '%1' from a data with different ids (%2 != %3 and %4 != %5).", resourceUid(), localId.toString(), data.resource.id.toString(), data.resource.path, m_resourceUid);
#endif

    bool result = true;

    /* Note that the order of these calls is important. */
    result &= setFlag(Qn::Pinned, false);
    result &= setCombinedGeometry(data.combinedGeometry);
    setRotation(data.rotation);
    result &= setFlags(static_cast<Qn::ItemFlags>(data.flags));

    return result;
}

void QnWorkbenchItem::submit(QnLayoutItemData &data) const {
    if(data.uuid != uuid())
        qnWarning("Submitting item '%1' to a data with different uuid (%2 != %3).", resourceUid(), data.uuid, uuid());

#ifdef _DEBUG
    QnId localId = qnResPool->getResourceByUniqId(resourceUid())->getId();
    if(data.resource.id != localId && data.resource.path != m_resourceUid)
        qnWarning("Submitting item '%1' to a data with different ids (%2 != %3 and %4 != %5).", resourceUid(), localId.toString(), data.resource.id.toString(), data.resource.path, m_resourceUid);
#endif

    data.flags = flags();
    data.rotation = rotation();
    data.combinedGeometry = combinedGeometry();
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

bool QnWorkbenchItem::setFlag(Qn::ItemFlag flag, bool value) {
    if(checkFlag(flag) == value)
        return true;

    if(m_layout != NULL) {
        if(flag == Qn::Pinned && value)
            return m_layout->pinItem(this, m_geometry);

        if(flag == Qn::Pinned && !value)
            return m_layout->unpinItem(this);
    }

    setFlagInternal(flag, value);
    return true;
}

bool QnWorkbenchItem::setFlags(Qn::ItemFlags flags) {
    if(m_flags == flags)
        return true;

    bool result = true;
    if((m_flags ^ flags) & Qn::Pinned)
        result &= setFlag(Qn::Pinned, flags & Qn::Pinned);
    if((m_flags ^ flags) & Qn::PendingGeometryAdjustment)
        result &= setFlag(Qn::PendingGeometryAdjustment, flags & Qn::PendingGeometryAdjustment);


    return result;
}

void QnWorkbenchItem::setFlagInternal(Qn::ItemFlag flag, bool value) {
    if(checkFlag(flag) == value)
        return;

    if(flag == Qn::Pinned && value)
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
    setFlag(Qn::PendingGeometryAdjustment, true);
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
    setFlag(Qn::PendingGeometryAdjustment, true);
}
