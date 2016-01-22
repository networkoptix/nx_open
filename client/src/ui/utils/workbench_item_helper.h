
#pragma once

#include <core/resource/resource_fwd.h>
#include <ui/workbench/workbench_item.h>
#include <core/resource/layout_item_data.h>
#include <core/resource_management/resource_pool.h>

namespace helpers
{
    template<typename ResourceType>
    QnSharedResourcePointer<ResourceType> extractResource(QnWorkbenchItem *item)
    {
        const auto layoutItemData = item->data();
        const auto id = layoutItemData.resource.id;
        return qnResPool->getResourceById<ResourceType>(id);
    }

    QnVirtualCameraResourcePtr extractCameraResource(QnWorkbenchItem *item);
}