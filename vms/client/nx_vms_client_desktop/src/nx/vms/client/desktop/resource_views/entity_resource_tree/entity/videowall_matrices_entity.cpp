// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "videowall_matrices_entity.h"

#include <core/resource/videowall_resource.h>
#include <nx/vms/client/desktop/resource_views/entity_item_model/item_order/item_order.h>

namespace nx::vms::client::desktop {
namespace entity_resource_tree {

VideowallMatricesEntity::VideowallMatricesEntity(
    const VideowallMatrixItemCreator& itemCreator,
    const QnVideoWallResourcePtr& videoWall)
    :
    base_type(itemCreator, entity_item_model::numericOrder())
{
    setItems(videoWall->matrices()->getItems().keys().toVector());

    m_connectionsGuard.add(videoWall->connect(videoWall.get(), &QnVideoWallResource::matrixAdded,
        [this](const QnVideoWallResourcePtr&, const QnVideoWallMatrix& matrix)
        {
            addItem(matrix.uuid);
        }));

    m_connectionsGuard.add(videoWall->connect(videoWall.get(), &QnVideoWallResource::matrixRemoved,
        [this](const QnVideoWallResourcePtr&, const QnVideoWallMatrix& matrix)
        {
            removeItem(matrix.uuid);
        }));
}

} // namespace entity_resource_tree
} // namesace nx::vms::client::desktop
