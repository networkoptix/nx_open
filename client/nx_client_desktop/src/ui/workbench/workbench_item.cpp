#include "workbench_item.h"

#include <limits>

#include <common/common_meta_types.h>
#include <core/resource/layout_resource.h>
#include <core/resource/media_resource.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource_management/resource_runtime_data.h>

#include "workbench_layout.h"


QnWorkbenchItem::QnWorkbenchItem(const QnResourcePtr& resource, const QnUuid& uuid, QObject* parent):
    QObject(parent),
    m_uuid(uuid),
    m_resource(resource)
{
    NX_ASSERT(m_resource);
    if (!m_resource)
        return;

    QString forcedRotation = resource->getProperty(QnMediaResource::rotationKey());
    if (!forcedRotation.isEmpty())
        m_rotation = forcedRotation.toInt();
}

QnWorkbenchItem::QnWorkbenchItem(const QnResourcePtr& resource,
    const QnLayoutItemData& data,
    QObject* parent)
    :
    QObject(parent),
    m_uuid(data.uuid),
    m_resource(resource)
{
    NX_EXPECT(m_resource && m_resource == resourcePool()->getResourceByDescriptor(data.resource));

    setFlags(static_cast<Qn::ItemFlags>(data.flags));
    setRotation(data.rotation);
    setCombinedGeometry(data.combinedGeometry);
    setZoomRect(data.zoomRect);
    setImageEnhancement(data.contrastParams);
    setDewarpingParams(data.dewarpingParams);
    setDisplayInfo(data.displayInfo);
}

QnWorkbenchItem::~QnWorkbenchItem()
{
    if (m_layout)
        m_layout->removeItem(this);
}

QnLayoutItemData QnWorkbenchItem::data() const
{
    NX_EXPECT(m_resource);

    QnLayoutItemData data;
    data.uuid = m_uuid;
    if (m_resource && m_resource->hasFlags(Qn::local_media))
        data.resource.uniqueId = m_resource->getUniqueId();
    data.resource.id = m_resource ? m_resource->getId() : QnUuid();
    data.flags = flags();
    data.rotation = rotation();
    data.combinedGeometry = combinedGeometry();
    data.zoomRect = zoomRect();
    data.contrastParams = imageEnhancement();
    data.dewarpingParams = dewarpingParams();
    data.zoomTargetUuid = zoomTargetItem() ? zoomTargetItem()->uuid() : QnUuid();
    data.displayInfo = displayInfo();

    return data;
}

bool QnWorkbenchItem::update(const QnLayoutItemData &data)
{
    NX_EXPECT(data.uuid == uuid());
    NX_EXPECT(m_resource == resourcePool()->getResourceByDescriptor(data.resource));

    bool result = true;

    /* Note that the order of these calls is important. */
    result &= setFlag(Qn::Pinned, false);
    result &= setCombinedGeometry(data.combinedGeometry);
    setRotation(data.rotation);
    setZoomRect(data.zoomRect);
    setImageEnhancement(data.contrastParams);
    setDewarpingParams(data.dewarpingParams);
    setDisplayInfo(data.displayInfo);
    result &= setFlags(static_cast<Qn::ItemFlags>(data.flags));

    return result;
}

void QnWorkbenchItem::submit(QnLayoutItemData &data) const
{
    NX_EXPECT(data.uuid == uuid());
    NX_EXPECT(m_resource == resourcePool()->getResourceByDescriptor(data.resource));

    data.flags = flags();
    data.rotation = rotation();
    data.zoomRect = zoomRect();
    data.contrastParams = imageEnhancement();
    data.dewarpingParams = dewarpingParams();
    data.zoomTargetUuid = zoomTargetItem() ? zoomTargetItem()->uuid() : QnUuid();
    data.combinedGeometry = combinedGeometry();
    data.displayInfo = displayInfo();
}

QnResourcePtr QnWorkbenchItem::resource() const
{
    return m_resource;
}

bool QnWorkbenchItem::setGeometry(const QRect &geometry)
{
    if (m_layout != NULL)
        return m_layout->moveItem(this, geometry);

    setGeometryInternal(geometry);
    return true;
}

void QnWorkbenchItem::setGeometryInternal(const QRect &geometry)
{
    if (m_geometry == geometry)
        return;

    m_geometry = geometry;

    emit geometryChanged();
    emit dataChanged(Qn::ItemGeometryRole);
    emit dataChanged(Qn::ItemCombinedGeometryRole);
}

bool QnWorkbenchItem::setGeometryDelta(const QRectF &geometryDelta)
{
    if (isPinned())
        return false;

    if (qFuzzyEquals(m_geometryDelta, geometryDelta))
        return true;

    m_geometryDelta = geometryDelta;

    emit geometryDeltaChanged();
    emit dataChanged(Qn::ItemGeometryDeltaRole);
    emit dataChanged(Qn::ItemCombinedGeometryRole);
    return true;
}

bool QnWorkbenchItem::setCombinedGeometry(const QRectF &combinedGeometry)
{
    QRect geometry = combinedGeometry.toRect(); /* Conversion uses rounding, so all differences are in [-0.5, 0.5]. */
    QRectF geometryDelta = QRectF(
        combinedGeometry.topLeft() - geometry.topLeft(),
        combinedGeometry.size() - geometry.size()
    );

    if (isPinned() && (!qFuzzyIsNull(geometryDelta.topLeft()) || !qFuzzyIsNull(geometryDelta.size())))
        return false; /* Cannot set geometry delta for pinned items. */

    if (!setGeometry(geometry))
        return false;

    if (!isPinned())
        setGeometryDelta(geometryDelta);

    return true;
}

QRectF QnWorkbenchItem::combinedGeometry() const
{
    return QRectF(
        m_geometry.left() + m_geometryDelta.left(),
        m_geometry.top() + m_geometryDelta.top(),
        m_geometry.width() + m_geometryDelta.width(),
        m_geometry.height() + m_geometryDelta.height()
    );
}

bool QnWorkbenchItem::setFlag(Qn::ItemFlag flag, bool value)
{
    if (hasFlag(flag) == value)
        return true;

    if (m_layout != NULL)
    {
        if (flag == Qn::Pinned && value)
            return m_layout->pinItem(this, m_geometry);

        if (flag == Qn::Pinned && !value)
            return m_layout->unpinItem(this);
    }

    setFlagInternal(flag, value);
    return true;
}

bool QnWorkbenchItem::setFlags(Qn::ItemFlags flags)
{
    if (m_flags == flags)
        return true;

    bool result = true;
    if ((m_flags ^ flags) & Qn::Pinned)
        result &= setFlag(Qn::Pinned, flags & Qn::Pinned);
    if ((m_flags ^ flags) & Qn::PendingGeometryAdjustment)
        result &= setFlag(Qn::PendingGeometryAdjustment, flags & Qn::PendingGeometryAdjustment);

    return result;
}

void QnWorkbenchItem::setFlagInternal(Qn::ItemFlag flag, bool value)
{
    if (hasFlag(flag) == value)
        return;

    if (flag == Qn::Pinned && value)
        setGeometryDelta(QRectF()); /* Pinned items cannot have non-zero geometry delta. */

    m_flags = value ? (m_flags | flag) : (m_flags & ~flag);

    emit flagChanged(flag, value);
    emit dataChanged(Qn::ItemFlagsRole);
}

void QnWorkbenchItem::setZoomRect(const QRectF &zoomRect)
{
    if (qFuzzyEquals(zoomRect, m_zoomRect))
        return;

    m_zoomRect = zoomRect;

    emit zoomRectChanged();
    emit dataChanged(Qn::ItemZoomRectRole);
}

void QnWorkbenchItem::setImageEnhancement(const ImageCorrectionParams& imageEnhancement)
{
    if (m_imageEnhancement == imageEnhancement)
        return;

    m_imageEnhancement = imageEnhancement;

    emit imageEnhancementChanged();
    emit dataChanged(Qn::ItemImageEnhancementRole);
}

void QnWorkbenchItem::setDewarpingParams(const QnItemDewarpingParams& params)
{
    if (m_itemDewarpingParams == params)
        return;

    m_itemDewarpingParams = params;
    emit dewarpingParamsChanged();
    emit dataChanged(Qn::ItemImageDewarpingRole);
}

QnWorkbenchItem *QnWorkbenchItem::zoomTargetItem() const
{
    if (m_layout)
    {
        return m_layout->zoomTargetItem(const_cast<QnWorkbenchItem *>(this));
    }
    else
    {
        return NULL;
    }
}

void QnWorkbenchItem::setRotation(qreal rotation)
{
    if (qFuzzyEquals(m_rotation, rotation))
        return;

    m_rotation = rotation;

    emit rotationChanged();
    emit dataChanged(Qn::ItemRotationRole);
}

bool QnWorkbenchItem::displayInfo() const
{
    return m_displayInfo;
}

void QnWorkbenchItem::setDisplayInfo(bool value)
{
    if (m_displayInfo == value)
        return;
    m_displayInfo = value;
    emit displayInfoChanged();
    emit dataChanged(Qn::ItemDisplayInfoRole);
}

void QnWorkbenchItem::adjustGeometry()
{
    /* Invalidate geometry. */
    QPoint topLeft = m_geometry.topLeft();
    setGeometry(QRect(topLeft + QPoint(1, 1), topLeft - QPoint(1, 1)));
    setGeometryDelta(QRectF());

    /* Set geometry adjustment flag. */
    setFlag(Qn::PendingGeometryAdjustment, true);
}

void QnWorkbenchItem::adjustGeometry(const QPointF &desiredPosition)
{
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

QVariant QnWorkbenchItem::data(Qn::ItemDataRole role) const
{
    switch (role)
    {
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
            return qnResourceRuntimeDataManager->layoutItemData(m_uuid, role);
    }
}

void QnWorkbenchItem::setData(Qn::ItemDataRole role, const QVariant &value)
{
    QVariant localValue = qnResourceRuntimeDataManager->layoutItemData(m_uuid, role);
    switch (role)
    {
        case Qn::ItemUuidRole:
            NX_ASSERT(value.value<QnUuid>() == uuid());
            break;
        case Qn::ItemGeometryRole:
            NX_ASSERT(value.canConvert<QRect>());
            setGeometry(value.toRect());
            break;
        case Qn::ItemGeometryDeltaRole:
            NX_ASSERT(value.canConvert<QRectF>());
            setGeometryDelta(value.toRectF());
            break;
        case Qn::ItemCombinedGeometryRole:
            NX_ASSERT(value.canConvert<QRectF>());
            setCombinedGeometry(value.toRectF());
            break;
        case Qn::ItemZoomRectRole:
            NX_ASSERT(value.canConvert<QRectF>());
            setZoomRect(value.toRectF());
            break;
        case Qn::ItemImageEnhancementRole:
            NX_ASSERT(value.canConvert<ImageCorrectionParams>());
            setImageEnhancement(value.value<ImageCorrectionParams>());
            break;
        case Qn::ItemImageDewarpingRole:
            NX_ASSERT(value.canConvert<QnItemDewarpingParams>());
            setDewarpingParams(value.value<QnItemDewarpingParams>());
            break;
        case Qn::ItemFlagsRole:
        {
            bool ok;
            int flags = value.toInt(&ok);
            NX_ASSERT(ok);
            setFlags(static_cast<Qn::ItemFlags>(flags));
        }
        case Qn::ItemRotationRole:
        {
            bool ok;
            qreal rotation = value.toReal(&ok);
            NX_ASSERT(ok);
            setRotation(rotation);
            break;
        }
        case Qn::ItemFlipRole:
        {
            /* Avoiding unnecessary dataChanged calls */
            bool flip = value.toBool();
            if (localValue.toBool() != flip)
            {
                qnResourceRuntimeDataManager->setLayoutItemData(m_uuid, role, flip);
                emit dataChanged(Qn::ItemFlipRole);
            }
            break;
        }
        default:
        {
            if (localValue == value)
                return;

            qnResourceRuntimeDataManager->setLayoutItemData(m_uuid, role, value);
            emit dataChanged(role);
            break;
        }
    }
}

void QnWorkbenchItem::sortByGeometry(QList<QnWorkbenchItem*>* items)
{
    NX_EXPECT(items);
    if (!items)
        return;

    std::sort(items->begin(), items->end(),
        [](QnWorkbenchItem* l, QnWorkbenchItem* r)
        {
            QRect lg = l->geometry();
            QRect rg = r->geometry();
            return lg.y() < rg.y() || (lg.y() == rg.y() && lg.x() < rg.x());
        });
}
