#include "radass_support.h"

#include <common/common_module.h>
#include <client_core/client_core_module.h>

#include <core/resource_management/resource_pool.h>
#include <core/resource/camera_resource.h>
#include <core/resource/layout_resource.h>
#include <core/resource/layout_item_data.h>
#include <core/resource/layout_item_index.h>

#include <nx/client/desktop/condition/generic_condition.h>


namespace nx {
namespace client {
namespace desktop {

namespace {

bool isRadassSupportedInternal(QnResourcePool* resourcePool, const QnLayoutItemData& item)
{
    if (!item.zoomRect.isNull())
        return false;

    // Some layouts do not belong to the same resource pool as cameras (e.g. showreels).
    if (!resourcePool)
    {
        // Unit tests do not have core module.
        if (!qnClientCoreModule)
            return false;

        resourcePool = qnClientCoreModule->commonModule()->resourcePool();
    }

    return isRadassSupported(resourcePool->getResourceByDescriptor(item.resource)
        .dynamicCast<QnVirtualCameraResource>());
}

} // namespace

bool isRadassSupported(const QnLayoutResourcePtr& layout, MatchMode match)
{
    if (!layout)
        return false;

    return GenericCondition::check<QnLayoutItemData>(layout->getItems(), match,
        [resourcePool = layout->resourcePool()](const QnLayoutItemData& item)
        {
            return isRadassSupportedInternal(resourcePool, item);
        });
}

ConditionResult isRadassSupported(const QnLayoutResourcePtr& layout)
{
    if (!layout)
        return ConditionResult::None;

    return GenericCondition::check<QnLayoutItemData>(layout->getItems(),
        [resourcePool = layout->resourcePool()](const QnLayoutItemData& item)
        {
            return isRadassSupportedInternal(resourcePool, item);
        });
}

bool isRadassSupported(const QnLayoutItemIndex& item)
{
    if (!item.layout())
        return false;

    const auto layoutItem = item.layout()->getItem(item.uuid());
    return isRadassSupportedInternal(item.layout()->resourcePool(), layoutItem);
}

bool isRadassSupported(const QnVirtualCameraResourcePtr& camera)
{
    return camera && camera->hasDualStreaming2();
}

bool isRadassSupported(const QnVirtualCameraResourceList& cameras, MatchMode match)
{
    return GenericCondition::check<QnVirtualCameraResourcePtr>(cameras, match,
        [](const QnVirtualCameraResourcePtr& camera)
        {
            return isRadassSupported(camera);
        });
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