// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "workbench_item.h"

#include <QtCore/QCollator>

#include <common/common_meta_types.h>
#include <core/resource/media_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/utils/math/fuzzy.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/layout/layout_data_helper.h>
#include <nx/vms/client/desktop/resource/layout_resource.h>
#include <nx/vms/client/desktop/system_context.h>

#include "workbench_layout.h"

using namespace nx::vms::client::desktop;

QnWorkbenchItem::QnWorkbenchItem(const QnResourcePtr& resource, const QnUuid& uuid, QObject* parent):
    QObject(parent),
    m_uuid(uuid),
    m_resource(resource)
{
    NX_ASSERT(m_resource);
    if (!m_resource)
        return;

    if (const auto mediaRes = resource.dynamicCast<QnMediaResource>())
    {
        if (const auto forcedRotation = mediaRes->forcedRotation())
            m_rotation = *forcedRotation;
    }
}

QnWorkbenchItem::QnWorkbenchItem(const QnResourcePtr& resource,
    const nx::vms::common::LayoutItemData& data,
    QObject* parent)
    :
    QObject(parent),
    m_uuid(data.uuid),
    m_resource(resource)
{
    NX_ASSERT(m_resource);
    NX_ASSERT_HEAVY_CONDITION(
        m_resource == resource->resourcePool()->getResourceByDescriptor(data.resource));

    setFlags(static_cast<Qn::ItemFlags>(data.flags));
    setRotation(data.rotation);
    setCombinedGeometry(data.combinedGeometry);
    setZoomRect(data.zoomRect);
    setImageEnhancement(data.contrastParams);
    setDewarpingParams(data.dewarpingParams);
    setDisplayInfo(data.displayInfo);
    setControlPtz(data.controlPtz);
    setDisplayAnalyticsObjects(data.displayAnalyticsObjects);
    setDisplayRoi(data.displayRoi);
    setDisplayHotspots(data.displayHotspots);
}

QnWorkbenchItem::~QnWorkbenchItem()
{
    if (m_layout)
        m_layout->removeItem(this);
}

void QnWorkbenchItem::setLayout(QnWorkbenchLayout* value)
{
    if (m_layout)
        m_layout->resource()->disconnect(this);

    m_layout = value;

    if (m_layout)
    {
        connect(m_layout->resource().get(), &LayoutResource::itemDataChanged, this,
            [this](const QnUuid& itemId, Qn::ItemDataRole role, const QVariant& /*value*/)
            {
                if (itemId == m_uuid)
                    emit dataChanged(role);
            });
    }
}

nx::vms::common::LayoutItemData QnWorkbenchItem::data() const
{
    if (!NX_ASSERT(m_resource) || !NX_ASSERT(m_layout))
        return {};

    nx::vms::common::LayoutItemData data = layoutItemFromResource(m_resource,
        /*forceCloud*/ m_layout->resource()->hasFlags(Qn::cross_system));
    data.uuid = m_uuid;
    submit(data);
    return data;
}

bool QnWorkbenchItem::update(const nx::vms::common::LayoutItemData& data)
{
    NX_ASSERT(data.uuid == uuid());
    NX_ASSERT(m_resource);
    NX_ASSERT_HEAVY_CONDITION(
        m_resource == m_resource->resourcePool()->getResourceByDescriptor(data.resource));

    bool result = true;

    /* Note that the order of these calls is important. */
    result &= setFlag(Qn::Pinned, false);
    result &= setCombinedGeometry(data.combinedGeometry);
    setRotation(data.rotation);
    setZoomRect(data.zoomRect);
    setImageEnhancement(data.contrastParams);
    setDewarpingParams(data.dewarpingParams);
    setDisplayInfo(data.displayInfo);
    setControlPtz(data.controlPtz);
    setDisplayAnalyticsObjects(data.displayAnalyticsObjects);
    setDisplayRoi(data.displayRoi);
    setDisplayHotspots(data.displayHotspots);
    result &= setFlags(static_cast<Qn::ItemFlags>(data.flags));

    return result;
}

void QnWorkbenchItem::submit(nx::vms::common::LayoutItemData& data) const
{
    NX_ASSERT(data.uuid == uuid());
    NX_ASSERT(m_resource);
    NX_ASSERT_HEAVY_CONDITION(
        m_resource == m_resource->resourcePool()->getResourceByDescriptor(data.resource));

    data.flags = flags();
    data.rotation = rotation();
    data.zoomRect = zoomRect();
    data.contrastParams = imageEnhancement();
    data.dewarpingParams = dewarpingParams();
    data.zoomTargetUuid = zoomTargetItem() ? zoomTargetItem()->uuid() : QnUuid();
    data.combinedGeometry = combinedGeometry();
    data.displayInfo = displayInfo();
    data.controlPtz = controlPtz();
    data.displayAnalyticsObjects = displayAnalyticsObjects();
    data.displayRoi = displayRoi();
    data.displayHotspots = displayHotspots();
}

QnResourcePtr QnWorkbenchItem::resource() const
{
    return m_resource;
}

bool QnWorkbenchItem::setGeometry(const QRect &geometry)
{
    if (m_layout != nullptr)
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

    if (m_layout != nullptr)
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

void QnWorkbenchItem::setImageEnhancement(const nx::vms::api::ImageCorrectionData& imageEnhancement)
{
    if (m_imageEnhancement == imageEnhancement)
        return;

    m_imageEnhancement = imageEnhancement;

    emit imageEnhancementChanged();
    emit dataChanged(Qn::ItemImageEnhancementRole);
}

void QnWorkbenchItem::setDewarpingParams(const nx::vms::api::dewarping::ViewData& params)
{
    if (m_itemDewarpingParams == params)
        return;

    // Suppress signals if dewarping parameters are disabled. They are not actual anyway. Otherwise
    // dataChanged will be propagated to layout synchronizer, which will queue changes and post them
    // to the snapshot manager, which will display '*'.
    const bool changesAreActual = m_itemDewarpingParams.enabled || params.enabled;
    m_itemDewarpingParams = params;
    if (!changesAreActual)
        return;

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
        return nullptr;
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

bool QnWorkbenchItem::controlPtz() const
{
    return m_controlPtz;
}

void QnWorkbenchItem::setControlPtz(bool value)
{
    if (m_controlPtz == value)
        return;

    m_controlPtz = value;
    emit controlPtzChanged();
    emit dataChanged(Qn::ItemPtzControlRole);
}

bool QnWorkbenchItem::displayAnalyticsObjects() const
{
    return m_displayAnalyticsObjects;
}

void QnWorkbenchItem::setDisplayAnalyticsObjects(bool value)
{
    if (m_displayAnalyticsObjects == value)
        return;

    m_displayAnalyticsObjects = value;
    emit displayAnalyticsObjectsChanged();
    emit dataChanged(Qn::ItemDisplayAnalyticsObjectsRole);
}

bool QnWorkbenchItem::displayRoi() const
{
    return m_displayRoi;
}

void QnWorkbenchItem::setDisplayRoi(bool value)
{
    if (m_displayRoi == value)
        return;

    m_displayRoi = value;
    emit displayRoiChanged();
    emit dataChanged(Qn::ItemDisplayRoiRole);
}

QColor QnWorkbenchItem::frameDistinctionColor() const
{
    return m_frameDistinctionColor;
}

void QnWorkbenchItem::setFrameDistinctionColor(const QColor& value)
{
    if (m_frameDistinctionColor == value)
        return;

    m_frameDistinctionColor = value;
    emit dataChanged(Qn::ItemFrameDistinctionColorRole);
}

bool QnWorkbenchItem::displayHotspots() const
{
    return m_displayHotspots;
}

void QnWorkbenchItem::setDisplayHotspots(bool value)
{
    if (m_displayHotspots == value)
        return;

    m_displayHotspots = value;
    emit displayHotspotsChanged();
    emit dataChanged(Qn::ItemDisplayHotspotsRole);
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
            return QVariant::fromValue<nx::vms::api::ImageCorrectionData>(imageEnhancement());
        case Qn::ItemImageDewarpingRole:
            return QVariant::fromValue<nx::vms::api::dewarping::ViewData>(dewarpingParams());
        case Qn::ItemFlagsRole:
            return static_cast<int>(flags());
        case Qn::ItemRotationRole:
            return rotation();
        case Qn::ItemDisplayHotspotsRole:
            return displayHotspots();
        default:
            break;
    }

    LayoutResourcePtr currentLayout;
    if (NX_ASSERT(layout()))
        currentLayout = layout()->resource();

    if (!NX_ASSERT(currentLayout))
        return QVariant();

    return currentLayout->itemData(m_uuid, role);
}

void QnWorkbenchItem::setData(Qn::ItemDataRole role, const QVariant &value)
{
    LayoutResourcePtr currentLayout;
    if (NX_ASSERT(layout()))
        currentLayout = layout()->resource();

    if (!NX_ASSERT(currentLayout))
        return;

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
            NX_ASSERT(value.canConvert<nx::vms::api::ImageCorrectionData>());
            setImageEnhancement(value.value<nx::vms::api::ImageCorrectionData>());
            break;
        case Qn::ItemImageDewarpingRole:
            NX_ASSERT(value.canConvert<nx::vms::api::dewarping::ViewData>());
            setDewarpingParams(value.value<nx::vms::api::dewarping::ViewData>());
            break;
        case Qn::ItemFlagsRole:
        {
            bool ok;
            int flags = value.toInt(&ok);
            NX_ASSERT(ok);
            setFlags(static_cast<Qn::ItemFlags>(flags));
            break;
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
            // Avoiding unnecessary dataChanged calls.
            bool flip = value.toBool();
            bool localValue = currentLayout->itemData(m_uuid, role).toBool();
            if (localValue != flip)
            {
                currentLayout->setItemData(m_uuid, role, flip);
                emit dataChanged(Qn::ItemFlipRole);
            }
            break;
        }
        case Qn::ItemDisplayHotspotsRole:
        {
            if (NX_ASSERT(!value.isNull()))
                setDisplayHotspots(value.toBool());
            break;
        }
        default:
        {
            auto localValue = currentLayout->itemData(m_uuid, role);
            if (localValue == value)
                return;

            currentLayout->setItemData(m_uuid, role, value);
            break;
        }
    }
}

void QnWorkbenchItem::sortByGeometryAndName(QList<QnWorkbenchItem*>& items)
{
    std::sort(items.begin(), items.end(),
        [](const QnWorkbenchItem* l, const QnWorkbenchItem* r)
        {
            const QRect lg = l->geometry();
            const QRect rg = r->geometry();

            if (lg.topLeft() == rg.topLeft())
            {
                QCollator collator;
                collator.setNumericMode(true);
                return collator.compare(l->resource()->getName(), r->resource()->getName()) < 0;
            }
            return lg.y() < rg.y() || (lg.y() == rg.y() && lg.x() < rg.x());
        });
}
