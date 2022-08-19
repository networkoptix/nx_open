// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "showreels_list_entity.h"

#include <core/resource_management/layout_tour_manager.h>
#include <nx/vms/api/data/layout_tour_data.h>
#include <nx/vms/client/desktop/resource_views/entity_item_model/item_order/item_order.h>

namespace nx::vms::client::desktop {
namespace entity_resource_tree {

using namespace nx::vms::api;

ShowreelsListEntity::ShowreelsListEntity(
    const ShowreelItemCreator& showreelItemCreator,
    const QnLayoutTourManager* showreelManager)
    :
    base_type(showreelItemCreator, entity_item_model::numericOrder())
{
    const auto showreels = showreelManager->tours();

    QVector<QnUuid> tourIds;
    std::transform(std::cbegin(showreels), std::cend(showreels), std::back_inserter(tourIds),
        [](const auto& tourData) { return tourData.id; });

    setItems(tourIds);

    m_connectionsGuard.add(
        showreelManager->connect(showreelManager, &QnLayoutTourManager::tourAdded,
        [this](const LayoutTourData& tour) { addItem(tour.id); }));

    m_connectionsGuard.add(
        showreelManager->connect(showreelManager, &QnLayoutTourManager::tourRemoved,
        [this](const QnUuid& tourId) { removeItem(tourId); }));
}

} // namespace entity_resource_tree
} // namespace nx::vms::client::desktop
