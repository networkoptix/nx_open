// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "workbench_item_helper.h"

#include <core/resource/camera_resource.h>
#include <nx/vms/client/desktop/resource/resource_descriptor.h>

using namespace nx::vms::client::desktop;

namespace helpers {

template<typename ResourceType>
QnSharedResourcePointer<ResourceType> extractResource(QnWorkbenchItem *item)
{
    const auto layoutItemData = item->data();
    return getResourceByDescriptor(layoutItemData.resource).template dynamicCast<ResourceType>();
}

QnVirtualCameraResourcePtr extractCameraResource(QnWorkbenchItem *item)
{
    return extractResource<QnVirtualCameraResource>(item);
}

QnTimePeriod extractItemTimeWindow(QnWorkbenchItem *item)
{
    QnTimePeriod window = item->data(Qn::ItemSliderWindowRole).value<QnTimePeriod>();

    if(window.isEmpty())
        window = QnTimePeriod::anytime();

    qint64 windowStartMs = window.startTimeMs;
    qint64 windowEndMs = (window.isInfinite() ? QnTimePeriod::kMaxTimeValue : window.endTimeMs());
    return QnTimePeriod::fromInterval(windowStartMs, windowEndMs);
}

} // namespace helpers
