
#pragma once

#include <ui/workbench/workbench_item.h>
#include <core/resource/resource_fwd.h>
#include <core/resource/layout_item_data.h>
#include <core/resource_management/resource_pool.h>

#include <recording/time_period.h>

namespace helpers
{
    template<typename ResourceType>
    QnSharedResourcePointer<ResourceType> extractResource(QnWorkbenchItem *item)
    {
        const auto layoutItemData = item->data();
        return qnResPool->getResourceByDescriptor(layoutItemData.resource).template dynamicCast<ResourceType>();

    }

    QnVirtualCameraResourcePtr extractCameraResource(QnWorkbenchItem *item);

    QnTimePeriod extractItemTimeWindow(QnWorkbenchItem *item);
}