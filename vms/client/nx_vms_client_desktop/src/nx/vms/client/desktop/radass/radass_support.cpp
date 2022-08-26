// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "radass_support.h"

#include <core/resource/camera_resource.h>
#include <core/resource/layout_item_data.h>
#include <core/resource/layout_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/vms/client/desktop/condition/generic_condition.h>
#include <nx/vms/client/desktop/resource/layout_item_index.h>
#include <nx/vms/client/desktop/resource/layout_resource.h>
#include <nx/vms/client/desktop/resource/resource_descriptor.h>

namespace nx::vms::client::desktop {

namespace {

bool isRadassSupportedInternal(const QnLayoutItemData& item)
{
    if (!item.zoomRect.isNull())
        return false;

    return isRadassSupported(
        getResourceByDescriptor(item.resource).dynamicCast<QnVirtualCameraResource>());
}

} // namespace

bool isRadassSupported(const QnLayoutResourcePtr& layout, MatchMode match)
{
    if (!layout)
        return false;

    return GenericCondition::check<QnLayoutItemData>(layout->getItems().values(), match,
        [](const QnLayoutItemData& item)
        {
            return isRadassSupportedInternal(item);
        });
}

ConditionResult isRadassSupported(const QnLayoutResourcePtr& layout)
{
    if (!layout)
        return ConditionResult::None;

    return GenericCondition::check<QnLayoutItemData>(layout->getItems().values(),
        [](const QnLayoutItemData& item)
        {
            return isRadassSupportedInternal(item);
        });
}

bool isRadassSupported(const LayoutItemIndex& item)
{
    if (!item.layout())
        return false;

    const auto layoutItem = item.layout()->getItem(item.uuid());
    return isRadassSupportedInternal(layoutItem);
}

bool isRadassSupported(const QnVirtualCameraResourcePtr& camera)
{
    return camera && camera->hasDualStreaming();
}

bool isRadassSupported(const QnVirtualCameraResourceList& cameras, MatchMode match)
{
    return GenericCondition::check<QnVirtualCameraResourcePtr>(cameras, match,
        [](const QnVirtualCameraResourcePtr& camera)
        {
            return isRadassSupported(camera);
        });
}

bool isRadassSupported(const LayoutItemIndexList& items, MatchMode match)
{
    return GenericCondition::check<LayoutItemIndex>(items, match,
        [](const LayoutItemIndex& item)
        {
            return isRadassSupported(item);
        });
}

ConditionResult isRadassSupported(const LayoutItemIndexList& items)
{
    return GenericCondition::check<LayoutItemIndex>(items,
        [](const LayoutItemIndex& item)
        {
            return isRadassSupported(item);
        });
}

} // namespace nx::vms::client::desktop
