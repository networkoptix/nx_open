// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "layout_item_adaptor.h"

#include <core/resource/layout_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/utils/log/assert.h>

#include "layout_model.h"

namespace nx::vms::client::desktop {

class LayoutItemAdaptor::Private
{
public:
    QnLayoutResourcePtr layout;
    common::LayoutItemData itemData;
};

LayoutItemAdaptor::LayoutItemAdaptor(const QnLayoutResourcePtr& layout, const nx::Uuid& itemId):
    d(new Private())
{
    NX_ASSERT(layout);
    NX_ASSERT(!itemId.isNull());

    d->layout = layout;

    d->itemData = layout->getItem(itemId);

    connect(d->layout.data(), &QnLayoutResource::itemChanged, this,
        [this](const QnLayoutResourcePtr& resource, const common::LayoutItemData& itemData)
        {
            NX_ASSERT(d->layout == resource);
            if (itemData.uuid != d->itemData.uuid)
                return;

            using Notifier = void(LayoutItemAdaptor::*)();

            QList<Notifier> notifiers;

            if (d->itemData.flags != itemData.flags)
                notifiers.append(&LayoutItemAdaptor::flagsChanged);

            if (d->itemData.combinedGeometry != itemData.combinedGeometry)
                notifiers.append(&LayoutItemAdaptor::geometryChanged);

            if (d->itemData.zoomTargetUuid != itemData.zoomTargetUuid)
                notifiers.append(&LayoutItemAdaptor::zoomTargetIdChanged);

            if (d->itemData.zoomRect != itemData.zoomRect)
                notifiers.append(&LayoutItemAdaptor::zoomRectChanged);

            if (!qFuzzyCompare(d->itemData.rotation, itemData.rotation))
                notifiers.append(&LayoutItemAdaptor::rotationChanged);

            if (d->itemData.displayInfo != itemData.displayInfo)
                notifiers.append(&LayoutItemAdaptor::displayInfoChanged);

            if (d->itemData.controlPtz != itemData.controlPtz)
                notifiers.append(&LayoutItemAdaptor::controlPtzChanged);

            if (d->itemData.displayAnalyticsObjects != itemData.displayAnalyticsObjects)
                notifiers.append(&LayoutItemAdaptor::displayAnalyticsObjectsChanged);

            if (d->itemData.displayRoi != itemData.displayRoi)
                notifiers.append(&LayoutItemAdaptor::displayRoiChanged);

            if (d->itemData.contrastParams != itemData.contrastParams)
                notifiers.append(&LayoutItemAdaptor::imageCorrectionParamsChanged);

            if (d->itemData.dewarpingParams != itemData.dewarpingParams)
                notifiers.append(&LayoutItemAdaptor::dewarpingParamsChanged);

            if (d->itemData.displayHotspots != itemData.displayHotspots)
                notifiers.append(&LayoutItemAdaptor::displayHotspotsChanged);

            d->itemData = itemData;

            for (const auto& notifier: notifiers)
                (this->*(notifier))();
    });
}

LayoutItemAdaptor::~LayoutItemAdaptor()
{
}

QnLayoutResourcePtr LayoutItemAdaptor::layout() const
{
    return d->layout;
}

QnLayoutResource* LayoutItemAdaptor::layoutPlainPointer() const
{
    return d->layout.data();
}

nx::Uuid LayoutItemAdaptor::itemId() const
{
    return d->itemData.uuid;
}

nx::Uuid LayoutItemAdaptor::resourceId() const
{
    return d->itemData.resource.id;
}

QnResourcePtr LayoutItemAdaptor::resource() const
{
    return d->layout->resourcePool()->getResourceById(d->itemData.resource.id);
}

QnResource* LayoutItemAdaptor::resourcePlainPointer() const
{
    return resource().data();
}

int LayoutItemAdaptor::flags() const
{
    return d->itemData.flags;
}

void LayoutItemAdaptor::setFlags(int flags)
{
    if (d->itemData.flags == flags)
        return;

    d->itemData.flags = flags;
    d->layout->updateItem(d->itemData);

    emit flagsChanged();
}

QRect LayoutItemAdaptor::geometry() const
{
    return d->itemData.combinedGeometry.toRect();
}

void LayoutItemAdaptor::setGeometry(const QRect& geometry)
{
    if (d->itemData.combinedGeometry.toRect() == geometry)
        return;

    d->itemData.combinedGeometry = geometry;
    d->layout->updateItem(d->itemData);

    emit geometryChanged();
}

nx::Uuid LayoutItemAdaptor::zoomTargetId() const
{
    return d->itemData.zoomTargetUuid;
}

void LayoutItemAdaptor::setZoomTargetId(const nx::Uuid& id)
{
    if (d->itemData.zoomTargetUuid == id)
        return;

    d->itemData.zoomTargetUuid = id;
    d->layout->updateItem(d->itemData);

    emit zoomTargetIdChanged();
}

QRectF LayoutItemAdaptor::zoomRect() const
{
    return d->itemData.zoomRect;
}

void LayoutItemAdaptor::setZoomRect(const QRectF& zoomRect)
{
    if (d->itemData.zoomRect == zoomRect)
        return;

    d->itemData.zoomRect = zoomRect;
    d->layout->updateItem(d->itemData);

    emit zoomRectChanged();
}

qreal LayoutItemAdaptor::rotation() const
{
    return d->itemData.rotation;
}

void LayoutItemAdaptor::setRotation(qreal rotation)
{
    if (qFuzzyCompare(d->itemData.rotation, rotation))
        return;

    d->itemData.rotation = rotation;
    d->layout->updateItem(d->itemData);

    emit rotationChanged();
}

bool LayoutItemAdaptor::displayInfo() const
{
    return d->itemData.displayInfo;
}

void LayoutItemAdaptor::setDisplayInfo(bool displayInfo)
{
    if (d->itemData.displayInfo == displayInfo)
        return;

    d->itemData.displayInfo = displayInfo;
    d->layout->updateItem(d->itemData);

    emit displayInfoChanged();
}

bool LayoutItemAdaptor::controlPtz() const
{
    return d->itemData.controlPtz;
}

void LayoutItemAdaptor::setControlPtz(bool controlPtz)
{
    if (d->itemData.controlPtz == controlPtz)
        return;

    d->itemData.controlPtz = controlPtz;
    d->layout->updateItem(d->itemData);

    emit controlPtzChanged();
}

bool LayoutItemAdaptor::displayAnalyticsObjects() const
{
    return d->itemData.displayAnalyticsObjects;
}

void LayoutItemAdaptor::setDisplayAnalyticsObjects(bool displayAnalyticsObjects)
{
    if (d->itemData.displayAnalyticsObjects == displayAnalyticsObjects)
        return;

    d->itemData.displayAnalyticsObjects = displayAnalyticsObjects;
    d->layout->updateItem(d->itemData);

    emit displayAnalyticsObjectsChanged();
}

bool LayoutItemAdaptor::displayRoi() const
{
    return d->itemData.displayRoi;
}

void LayoutItemAdaptor::setDisplayRoi(bool displayRoi)
{
    if (d->itemData.displayRoi == displayRoi)
        return;

    d->itemData.displayRoi = displayRoi;
    d->layout->updateItem(d->itemData);

    emit displayRoiChanged();
}

bool LayoutItemAdaptor::displayHotspots() const
{
    return d->itemData.displayHotspots;
}

void LayoutItemAdaptor::setDisplayHotspots(bool displayHotspots)
{
    if (d->itemData.displayHotspots == displayHotspots)
        return;

    d->itemData.displayHotspots = displayHotspots;
    d->layout->updateItem(d->itemData);

    emit displayHotspotsChanged();
}

nx::vms::api::ImageCorrectionData LayoutItemAdaptor::imageCorrectionParams() const
{
    return d->itemData.contrastParams;
}

void LayoutItemAdaptor::setImageCorrectionParams(const nx::vms::api::ImageCorrectionData& params)
{
    if (d->itemData.contrastParams == params)
        return;

    d->itemData.contrastParams = params;
    d->layout->updateItem(d->itemData);

    emit imageCorrectionParamsChanged();
}

nx::vms::api::dewarping::ViewData LayoutItemAdaptor::dewarpingParams() const
{
    return d->itemData.dewarpingParams;
}

void LayoutItemAdaptor::setDewarpingParams(const nx::vms::api::dewarping::ViewData& params)
{
    if (d->itemData.dewarpingParams == params)
        return;

    d->itemData.dewarpingParams = params;
    d->layout->updateItem(d->itemData);

    emit dewarpingParamsChanged();
}

} // namespace nx::vms::client::desktop
