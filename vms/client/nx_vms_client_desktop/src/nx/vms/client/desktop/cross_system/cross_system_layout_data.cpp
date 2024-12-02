// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "cross_system_layout_data.h"

#include <nx/vms/client/desktop/resource/resource_descriptor.h>
#include <nx/vms/client/desktop/resource_views/entity_resource_tree/resource_grouping/resource_grouping.h>
#include <nx_ec/data/api_conversion_functions.h>

#include "cross_system_layout_resource.h"

namespace nx::vms::client::desktop {

void fromDataToResource(
    const CrossSystemLayoutData& data,
    const CrossSystemLayoutResourcePtr& resource)
{
    resource->setIdUnsafe(data.id);
    resource->setName(data.name);
    resource->setCellAspectRatio(data.cellAspectRatio);
    resource->setCellSpacing(data.cellSpacing);
    resource->setLocked(data.locked);
    resource->setFixedSize({data.fixedWidth, data.fixedHeight});

    common::LayoutItemDataList dstItems;
    for (const CrossSystemLayoutItemData& srcItem: data.items)
    {
        common::LayoutItemData itemData;
        ec2::fromApiToResource(srcItem, itemData);
        itemData.resource.name = srcItem.name;
        dstItems.push_back(itemData);
    }
    resource->setItems(dstItems);

    entity_resource_tree::resource_grouping::setResourceCustomGroupId(resource,
        data.customGroupId);
}

void fromResourceToData(
    const CrossSystemLayoutResourcePtr& resource,
    CrossSystemLayoutData& data)
{
    data.id = resource->getId();
    data.name = resource->getName();
    data.cellAspectRatio = resource->cellAspectRatio();
    data.cellSpacing = resource->cellSpacing();
    data.locked = resource->locked();

    const auto fixedSize = resource->fixedSize();
    data.fixedWidth = fixedSize.isEmpty() ? 0 : fixedSize.width();
    data.fixedHeight = fixedSize.isEmpty() ? 0 : fixedSize.height();

    const common::LayoutItemDataMap& srcItems = resource->getItems();
    data.items.reserve(srcItems.size());

    for (const common::LayoutItemData& item: srcItems)
    {
        CrossSystemLayoutItemData itemData;
        ec2::fromResourceToApi(item, itemData);

        if (auto resource = getResourceByDescriptor(item.resource))
            itemData.name = resource->getName();
        else
            itemData.name = item.resource.name;

        data.items.push_back(itemData);
    }
    data.customGroupId = entity_resource_tree::resource_grouping::resourceCustomGroupId(resource);
}

} // namespace nx::vms::client::desktop
