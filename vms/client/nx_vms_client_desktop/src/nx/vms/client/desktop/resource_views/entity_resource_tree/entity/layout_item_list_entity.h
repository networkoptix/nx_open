// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <core/resource/resource_fwd.h>
#include <nx/utils/scoped_connections.h>
#include <nx/vms/client/desktop/resource_views/entity_item_model/entity/unique_key_list_entity.h>
#include <nx/vms/client/desktop/resource_views/entity_item_model/item/abstract_item.h>

namespace nx::vms::client::desktop {
namespace entity_resource_tree {

using LayoutItemCreator = std::function<entity_item_model::AbstractItemPtr(const nx::Uuid&)>;

class LayoutItemListEntity: public entity_item_model::UniqueKeyListEntity<nx::Uuid>
{
    using base_type = entity_item_model::UniqueKeyListEntity<nx::Uuid>;

public:
    LayoutItemListEntity(
        const QnLayoutResourcePtr& layout,
        const LayoutItemCreator& layoutItemCreator);

private:
    nx::utils::ScopedConnections m_connectionsGuard;
};

} // namespace entity_resource_tree
} // namespace nx::vms::client::desktop
