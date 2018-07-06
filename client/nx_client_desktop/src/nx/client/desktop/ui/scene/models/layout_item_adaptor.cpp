#include "layout_item_adaptor.h"

#include <nx/utils/log/assert.h>
#include <core/resource/layout_resource.h>
#include <core/resource_management/resource_pool.h>

#include "layout_model.h"

namespace nx {
namespace client {
namespace desktop {

class LayoutItemAdaptor::Private
{
public:
    QnLayoutResourcePtr layout;
    QnLayoutItemData itemData;
};

LayoutItemAdaptor::LayoutItemAdaptor(const QnLayoutResourcePtr& layout, const QnUuid& itemId):
    d(new Private())
{
    NX_EXPECT(layout);
    NX_EXPECT(!itemId.isNull());

    d->layout = layout;

    d->itemData = layout->getItem(itemId);

    connect(d->layout.data(), &QnLayoutResource::itemChanged, this,
        [this](const QnLayoutResourcePtr& resource, const QnLayoutItemData& itemData)
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

            if (d->itemData.contrastParams != itemData.contrastParams)
                notifiers.append(&LayoutItemAdaptor::imageCorrectionParamsChanged);

            if (d->itemData.dewarpingParams != itemData.dewarpingParams)
                notifiers.append(&LayoutItemAdaptor::dewarpingParamsChanged);

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

QnUuid LayoutItemAdaptor::itemId() const
{
    return d->itemData.uuid;
}

QnUuid LayoutItemAdaptor::resourceId() const
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

QnUuid LayoutItemAdaptor::zoomTargetId() const
{
    return d->itemData.zoomTargetUuid;
}

void LayoutItemAdaptor::setZoomTargetId(const QnUuid& id)
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

ImageCorrectionParams LayoutItemAdaptor::imageCorrectionParams() const
{
    return d->itemData.contrastParams;
}

void LayoutItemAdaptor::setImageCorrectionParams(const ImageCorrectionParams& params)
{
    if (d->itemData.contrastParams == params)
        return;

    d->itemData.contrastParams = params;
    d->layout->updateItem(d->itemData);

    emit imageCorrectionParamsChanged();
}

QnItemDewarpingParams LayoutItemAdaptor::dewarpingParams() const
{
    return d->itemData.dewarpingParams;
}

void LayoutItemAdaptor::setDewarpingParams(const QnItemDewarpingParams& params)
{
    if (d->itemData.dewarpingParams == params)
        return;

    d->itemData.dewarpingParams = params;
    d->layout->updateItem(d->itemData);

    emit dewarpingParamsChanged();
}

} // namespace desktop
} // namespace client
} // namespace nx
