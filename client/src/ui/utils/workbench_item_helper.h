
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
        const auto id = layoutItemData.resource.id;

        return (id.isNull()
            ? qnResPool->getResourceByUniqueId<ResourceType>(layoutItemData.resource.path)
            : qnResPool->getResourceById<ResourceType>(id));
    }

    QnVirtualCameraResourcePtr extractCameraResource(QnWorkbenchItem *item);

    QnTimePeriod extractItemTimeWindow(QnWorkbenchItem *item);
}