// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "videowall_screens_entity.h"

#include <core/resource/videowall_resource.h>
#include <nx/vms/client/desktop/resource_views/entity_item_model/item_order/item_order.h>

namespace nx::vms::client::desktop {
namespace entity_resource_tree {

VideoWallScreensEntity::VideoWallScreensEntity(
    const VideoWallScreenItemCreator& itemCreator,
    const QnVideoWallResourcePtr& videoWall)
    :
    base_type(itemCreator, entity_item_model::numericOrder())
{
    setItems(videoWall->items()->getItems().keys().toVector());

    m_connectionsGuard.add(videoWall->connect(videoWall.get(), &QnVideoWallResource::itemAdded,
        [this](const QnVideoWallResourcePtr&, const QnVideoWallItem& item)
        {
            addItem(item.uuid);
        }));

    m_connectionsGuard.add(videoWall->connect(videoWall.get(), &QnVideoWallResource::itemRemoved,
        [this](const QnVideoWallResourcePtr&, const QnVideoWallItem& item)
        {
            removeItem(item.uuid);
        }));
}

} // namespace entity_resource_tree
} // namespace nx::vms::client::desktop
