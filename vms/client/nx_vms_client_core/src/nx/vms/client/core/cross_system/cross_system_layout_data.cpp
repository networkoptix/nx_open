// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "cross_system_layout_data.h"

#include <core/resource/layout_resource.h>
#include <nx/utils/log/assert.h>
#include <nx/vms/client/core/resource/resource_descriptor_helpers.h>
#include <nx_ec/data/api_conversion_functions.h>

namespace nx::vms::client::core {

void fromDataToResource(
    const CrossSystemLayoutData& data,
    const QnLayoutResourcePtr& resource)
{
    NX_ASSERT(resource && resource->hasFlags(Qn::cross_system));

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
    resource->setProperty(api::resource_properties::kCustomGroupIdPropertyKey, data.customGroupId);
}

void fromResourceToData(
    const QnLayoutResourcePtr& resource,
    CrossSystemLayoutData& data)
{
    NX_ASSERT(resource && resource->hasFlags(Qn::cross_system));

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

        if (auto resource = core::getResourceByDescriptor(item.resource))
            itemData.name = resource->getName();
        else
            itemData.name = item.resource.name;

        data.items.push_back(itemData);
    }

    data.customGroupId =
        resource->getProperty(api::resource_properties::kCustomGroupIdPropertyKey);
}

} // namespace nx::vms::client::desktop
