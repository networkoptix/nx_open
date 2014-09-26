#include "workbench_item.h"

#include <limits>

#include <common/common_meta_types.h>
#include <core/resource/layout_resource.h>
#include <core/resource/media_resource.h>
#include <core/resource_management/resource_pool.h>

#include <ui/common/geometry.h>

#include "workbench_layout.h"


QnWorkbenchItem::QnWorkbenchItem(const QString &resourceUid, const QnUuid &uuid, QObject *parent):
    QObject(parent),
    m_layout(NULL),
    m_resourceUid(resourceUid),
    m_uuid(uuid),
    m_flags(0),
    m_rotation(0.0)
{
    if(resourceUid.isEmpty())
        return;

    QnResourcePtr resource = qnResPool->getResourceByUniqId(resourceUid);
    if(!resource)
        return;

    QString forcedRotation = resource->getProperty(QnMediaResource::rotationKey());
    if (!forcedRotation.isEmpty()) 
        m_rotation = forcedRotation.toInt();
}

QnWorkbenchItem::QnWorkbenchItem(const QnLayoutItemData &data, QObject *parent):
    QObject(parent),
    m_layout(NULL),
    m_resourceUid(data.resource.path),
    m_uuid(data.uuid),
    m_flags(0),
    m_rotation(0.0)
{
    if(m_resourceUid.isEmpty()) {
        qnWarning("Creating a workbench item from item data with invalid unique id.");
        // TODO: #Elric fix layout item data conventions.

        QnResourcePtr resource = qnResPool->getResourceById(data.resource.id);
        if(resource)
            m_resourceUid = resource->getUniqueId(); // TODO: #Elric add warning if NULL?
    }

    setFlags(static_cast<Qn::ItemFlags>(data.flags));
    setRotation(data.rotation);
    setCombinedGeometry(data.combinedGeometry);
    setZoomRect(data.zoomRect);
    setImageEnhancement(data.contrastParams);
    setDewarpingParams(data.dewarpingParams);

    m_dataByRole = data.dataByRole; // TODO: #Elric
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
    data.resource.id = resource ? resource->getId() : QnUuid();
    data.flags = flags();
    data.rotation = rotation();
    data.combinedGeometry = combinedGeometry();
    data.zoomRect = zoomRect();
    data.contrastParams = imageEnhancement();
    data.dewarpingParams = dewarpingParams();
    data.zoomTargetUuid = zoomTargetItem() ? zoomTargetItem()->uuid() : QnUuid();
    data.dataByRole = m_dataByRole;

    return data;
}

bool QnWorkbenchItem::update(const QnLayoutItemData &data) {
    if(data.uuid != uuid())
        qnWarning("Updating item '%1' from data with different uuid (%2 != %3).", resourceUid(), data.uuid.toString(), uuid().toString());

#ifdef _DEBUG
    QnResourcePtr resource = qnResPool->getResourceByUniqId(resourceUid());
    QnUuid localId = resource ? resource->getId() : QnUuid();
    if(data.resource.id != localId && data.resource.path != m_resourceUid)
        qnWarning("Updating item '%1' from a data with different ids (%2 != %3 and %4 != %5).", resourceUid(), localId.toString(), data.resource.id.toString(), data.resource.path, m_resourceUid);
#endif

    bool result = true;

    /* Note that the order of these calls is important. */
    result &= setFlag(Qn::Pinned, false);
    result &= setCombinedGeometry(data.combinedGeometry);
    setRotation(data.rotation);
    setZoomRect(data.zoomRect);
    setImageEnhancement(data.contrastParams);
    setDewarpingParams(data.dewarpingParams);
    result &= setFlags(static_cast<Qn::ItemFlags>(data.flags));

    m_dataByRole = data.dataByRole; // TODO

    return result;
}

void QnWorkbenchItem::submit(QnLayoutItemData &data) const {
    if(data.uuid != uuid())
        qnWarning("Submitting item '%1' to a data with different uuid (%2 != %3).", resourceUid(), data.uuid, uuid());

#ifdef _DEBUG
    QnUuid localId = qnResPool->getResourceByUniqId(resourceUid())->getId();
    if(data.resource.id != localId && data.resource.path != m_resourceUid)
        qnWarning("Submitting item '%1' to a data with different ids (%2 != %3 and %4 != %5).", resourceUid(), localId.toString(), data.resource.id.toString(), data.resource.path, m_resourceUid);
#endif

    data.flags = flags();
    data.rotation = rotation();
    data.zoomRect = zoomRect();
    data.contrastParams = imageEnhancement();
    data.dewarpingParams = dewarpingParams();
    data.zoomTargetUuid = zoomTargetItem() ? zoomTargetItem()->uuid() : QnUuid();
    data.combinedGeometry = combinedGeometry();
    data.dataByRole = m_dataByRole;
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
    emit dataChanged(Qn::ItemGeometryRole);
    emit dataChanged(Qn::ItemCombinedGeometryRole);
}

bool QnWorkbenchItem::setGeometryDelta(const QRectF &geometryDelta) {
    if(isPinned())
        return false;

    if(qFuzzyEquals(m_geometryDelta, geometryDelta))
        return true;

    m_geometryDelta = geometryDelta;

    emit geometryDeltaChanged();
    emit dataChanged(Qn::ItemGeometryDeltaRole);
    emit dataChanged(Qn::ItemCombinedGeometryRole);
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
    if(hasFlag(flag) == value)
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
    if(hasFlag(flag) == value)
        return;

    if(flag == Qn::Pinned && value)
        setGeometryDelta(QRectF()); /* Pinned items cannot have non-zero geometry delta. */

    m_flags = value ? (m_flags | flag) : (m_flags & ~flag);

    emit flagChanged(flag, value);
    emit dataChanged(Qn::ItemFlagsRole);
}

void QnWorkbenchItem::setZoomRect(const QRectF &zoomRect) {
    if(qFuzzyEquals(zoomRect, m_zoomRect))
        return;

    m_zoomRect = zoomRect;

    emit zoomRectChanged();
    emit dataChanged(Qn::ItemZoomRectRole);
}

void QnWorkbenchItem::setImageEnhancement(const ImageCorrectionParams& imageEnhancement)
{
    if(m_imageEnhancement == imageEnhancement)
        return;

    m_imageEnhancement = imageEnhancement;

    emit imageEnhancementChanged();
    emit dataChanged(Qn::ItemImageEnhancementRole);
}

void QnWorkbenchItem::setDewarpingParams(const QnItemDewarpingParams& params) {
    if(m_itemDewarpingParams == params)
        return;

    m_itemDewarpingParams = params;
    emit dewarpingParamsChanged();
    emit dataChanged(Qn::ItemImageDewarpingRole);
}

QnWorkbenchItem *QnWorkbenchItem::zoomTargetItem() const {
    if(m_layout) {
        return m_layout->zoomTargetItem(const_cast<QnWorkbenchItem *>(this));
    } else {
        return NULL;
    }
}

void QnWorkbenchItem::setRotation(qreal rotation) {
    if(qFuzzyCompare(m_rotation, rotation))
        return;

    m_rotation = rotation;

    emit rotationChanged();
    emit dataChanged(Qn::ItemRotationRole);
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

QVariant QnWorkbenchItem::data(int role) const {
    switch(role) {
    case Qn::ResourceUidRole:
        return resourceUid();
    case Qn::ItemUuidRole:
        return QVariant::fromValue<QnUuid>(uuid());
    case Qn::ItemGeometryRole:
        return geometry();
    case Qn::ItemGeometryDeltaRole:
        return geometryDelta();
    case Qn::ItemCombinedGeometryRole:
        return combinedGeometry();
    case Qn::ItemZoomRectRole:
        return zoomRect();
    case Qn::ItemImageEnhancementRole:
        return QVariant::fromValue<ImageCorrectionParams>(imageEnhancement());
    case Qn::ItemImageDewarpingRole:
        return QVariant::fromValue<QnItemDewarpingParams>(dewarpingParams());
    case Qn::ItemFlagsRole:
        return static_cast<int>(flags());
    case Qn::ItemRotationRole:
        return rotation();
    default:
        return m_dataByRole.value(role);
    }
}

bool QnWorkbenchItem::setData(int role, const QVariant &value) {
    switch(role) {
    case Qn::ResourceUidRole:
        if(value.toString() == resourceUid()) {
            return true;
        } else {
            qnWarning("Changing resource unique id of a workbench item is not supported.");
            return false;
        }
    case Qn::ItemUuidRole:
        if(value.value<QnUuid>() == uuid()) {
            return true;
        } else {
            qnWarning("Changing UUID of a workbench item is not supported.");
            return false;
        }
        break;
    case Qn::ItemGeometryRole:
        if(value.canConvert<QRect>()) {
            return setGeometry(value.toRect());
        } else {
            qnWarning("Provided geometry value '%1' is not convertible to QRect.", value);
            return false;
        }
    case Qn::ItemGeometryDeltaRole:
        if(value.canConvert<QRectF>()) {
            return setGeometryDelta(value.toRectF());
        } else {
            qnWarning("Provided geometry delta value '%1' is not convertible to QRectF.", value);
            return false;
        }
    case Qn::ItemCombinedGeometryRole:
        if(value.canConvert<QRectF>()) {
            return setCombinedGeometry(value.toRectF());
        } else {
            qnWarning("Provided combined geometry value '%1' is not convertible to QRectF.", value);
            return false;
        }
    case Qn::ItemZoomRectRole: {
        if(value.canConvert<QRectF>()) {
            setZoomRect(value.toRectF());
            return true;
        } else {
            qnWarning("Provided zoom rect value '%1' is not convertible to QRectF.", value);
            return false;
        }
    }

    case Qn::ItemImageEnhancementRole: {
        if(value.canConvert<ImageCorrectionParams>()) {
            setImageEnhancement(value.value<ImageCorrectionParams>());
            return true;
        } else {
            qnWarning("Provided image enhancment params is not convertible.", value);
            return false;
        }
    }

    case Qn::ItemImageDewarpingRole: {
        if(value.canConvert<QnItemDewarpingParams>()) {
            setDewarpingParams(value.value<QnItemDewarpingParams>());
            return true;
        } else {
            qnWarning("Provided dewarping params is not convertible.", value);
            return false;
        }
    }
                                       
    case Qn::ItemFlagsRole: {
        bool ok;
        int flags = value.toInt(&ok);
        if(ok) {
            return setFlags(static_cast<Qn::ItemFlags>(flags));
        } else {
            qnWarning("Provided flags value '%1' is not convertible to int.", value);
            return false;
        }
    }
    case Qn::ItemRotationRole: {
        bool ok;
        qreal rotation = value.toReal(&ok);
        if(ok) {
            setRotation(rotation);
            return true;
        } else {
            qnWarning("Provided rotation value '%1' must be convertible to qreal.", value);
            return false;
        }
    }
    default:
        QVariant &localValue = m_dataByRole[role];
        if(localValue != value) {
            localValue = value;
            emit dataChanged(role);
        }
        return true;
    }
}
