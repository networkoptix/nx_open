#include "layout_tour_item.h"

#include <nx_ec/data/api_layout_tour_data.h>

#include <core/resource_management/resource_pool.h>

#include <core/resource/layout_resource.h>

#include <nx/utils/log/assert.h>

QnLayoutTourItem::QnLayoutTourItem(
    const ec2::ApiLayoutTourItemData& data,
    QnResourcePool* resourcePool)
    :
    layout(resourcePool
        ? resourcePool->getResourceById<QnLayoutResource>(data.layoutId)
        : QnLayoutResourcePtr()),
    delayMs(data.delayMs)
{
    NX_EXPECT(resourcePool);
}

QnLayoutTourItemList QnLayoutTourItem::createList(const ec2::ApiLayoutTourItemDataList& items,
    QnResourcePool* resourcePool)
{
    QnLayoutTourItemList result;
    NX_EXPECT(resourcePool);
    if (!resourcePool)
        return result;

    result.reserve(items.size());
    for (const auto& item: items)
    {
        if (const auto& layout = resourcePool->getResourceById<QnLayoutResource>(item.layoutId))
            result.emplace_back(layout, item.delayMs);
    }
    return result;
}
