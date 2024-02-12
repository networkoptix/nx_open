// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "showreels_list_entity.h"

#include <nx/vms/api/data/showreel_data.h>
#include <nx/vms/client/desktop/resource_views/entity_item_model/item_order/item_order.h>
#include <nx/vms/common/showreel/showreel_manager.h>

namespace nx::vms::client::desktop {
namespace entity_resource_tree {

using namespace nx::vms::api;

ShowreelsListEntity::ShowreelsListEntity(
    const ShowreelItemCreator& showreelItemCreator,
    const common::ShowreelManager* showreelManager)
    :
    base_type(showreelItemCreator, entity_item_model::numericOrder())
{
    const auto showreels = showreelManager->showreels();

    QVector<nx::Uuid> showreelIds;
    for (const auto& showreel: showreels)
        showreelIds.push_back(showreel.id);

    setItems(showreelIds);

    m_connectionsGuard.add(
        showreelManager->connect(showreelManager, &common::ShowreelManager::showreelAdded,
        [this](const ShowreelData& showreel) { addItem(showreel.id); }));

    m_connectionsGuard.add(
        showreelManager->connect(showreelManager, &common::ShowreelManager::showreelRemoved,
        [this](const nx::Uuid& showreelId) { removeItem(showreelId); }));
}

} // namespace entity_resource_tree
} // namespace nx::vms::client::desktop
