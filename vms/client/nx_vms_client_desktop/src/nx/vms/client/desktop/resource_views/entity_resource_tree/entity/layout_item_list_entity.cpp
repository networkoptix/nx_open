// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "layout_item_list_entity.h"

#include <core/resource/layout_resource.h>
#include <client/client_globals.h>
#include <nx/vms/client/desktop/resource_views/entity_resource_tree/item_order/resource_tree_item_order.h>

namespace nx::vms::client::desktop {
namespace entity_resource_tree {

LayoutItemListEntity::LayoutItemListEntity(
    const QnLayoutResourcePtr& layout,
    const LayoutItemCreator& layoutItemCreator)
    :
    base_type(layoutItemCreator, layoutItemsOrder())
{
    setItems(layout->getItems().keys().toVector());

    m_connectionsGuard.add(layout->connect(layout.get(), &QnLayoutResource::itemAdded,
        [this](const QnLayoutResourcePtr&, const QnLayoutItemData& item)
        {
            addItem(item.uuid);
        }));

    m_connectionsGuard.add(layout->connect(layout.get(), &QnLayoutResource::itemRemoved,
        [this](const QnLayoutResourcePtr&, const QnLayoutItemData& item)
        {
            removeItem(item.uuid);
        }));
}

} // namespace entity_resource_tree
} // namespace nx::vms::client::desktop
