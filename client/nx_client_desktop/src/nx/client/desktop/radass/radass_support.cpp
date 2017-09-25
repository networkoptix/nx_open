#include "radass_support.h"

#include <core/resource_management/resource_pool.h>
#include <core/resource/camera_resource.h>
#include <core/resource/layout_resource.h>
#include <core/resource/layout_item_data.h>
#include <core/resource/layout_item_index.h>

#include <nx/client/desktop/condition/generic_condition.h>

namespace {

bool isRadassSupportedInternal(QnResourcePool* resourcePool, const QnLayoutItemData& item)
{
    if (!item.zoomRect.isNull())
        return false;

    const auto camera = resourcePool->getResourceByDescriptor(item.resource)
        .dynamicCast<QnVirtualCameraResource>();
    return camera && camera->hasDualStreaming2();
}

} // namespace

namespace nx {
namespace client {
namespace desktop {

bool isRadassSupported(const QnLayoutResourcePtr& layout, MatchMode match)
{
    if (!layout || !layout->resourcePool())
        return false;

    return GenericCondition::check<QnLayoutItemData>(layout->getItems(), match,
        [resourcePool = layout->resourcePool()](const QnLayoutItemData& item)
        {
            return isRadassSupportedInternal(resourcePool, item);
        });
}

ConditionResult isRadassSupported(const QnLayoutResourcePtr& layout)
{
    if (!layout || !layout->resourcePool())
        return ConditionResult::None;

    return GenericCondition::check<QnLayoutItemData>(layout->getItems(),
        [resourcePool = layout->resourcePool()](const QnLayoutItemData& item)
        {
            return isRadassSupportedInternal(resourcePool, item);
        });
}

bool isRadassSupported(const QnLayoutItemIndex& item)
{
    if (!item.layout() || !item.layout()->resourcePool())
        return false;

    const auto layoutItem = item.layout()->getItem(item.uuid());
    return isRadassSupportedInternal(item.layout()->resourcePool(), layoutItem);
}

bool isRadassSupported(const QnLayoutItemIndexList& items, MatchMode match)
{
    return GenericCondition::check<QnLayoutItemIndex>(items, match,
        [](const QnLayoutItemIndex& item)
        {
            return isRadassSupported(item);
        });
}

ConditionResult isRadassSupported(const QnLayoutItemIndexList& items)
{
    return GenericCondition::check<QnLayoutItemIndex>(items,
        [](const QnLayoutItemIndex& item)
        {
            return isRadassSupported(item);
        });
}

} // namespace desktop
} // namespace client
} // namespace nx