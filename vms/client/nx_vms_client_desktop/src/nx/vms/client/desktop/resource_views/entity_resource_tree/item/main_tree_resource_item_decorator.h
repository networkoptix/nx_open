// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <optional>

#include <common/common_globals.h>
#include <nx/utils/uuid.h>
#include <nx/vms/api/types/access_rights_types.h>
#include <nx/vms/client/desktop/resource_views/data/resource_tree_globals.h>
#include <nx/vms/client/desktop/resource_views/entity_item_model/item/abstract_item.h>

namespace nx::vms::client::desktop {
namespace entity_resource_tree {

class MainTreeResourceItemDecorator: public entity_item_model::AbstractItem
{
    using base_type = entity_item_model::AbstractItem;

public:
    struct Permissions
    {
        Qn::Permissions permissions;
        bool hasPowerUserPermissions = false;
    };

public:
    MainTreeResourceItemDecorator(
        entity_item_model::AbstractItemPtr sourceItem,
        Permissions permissions,
        ResourceTree::NodeType nodeType,
        const std::optional<nx::Uuid>& itemUuid = std::optional<nx::Uuid>());

    virtual QVariant data(int role) const override;
    virtual Qt::ItemFlags flags() const override;

private:
    entity_item_model::AbstractItemPtr m_sourceItem;
    Permissions m_permissions;
    ResourceTree::NodeType m_nodeType;
    std::optional<nx::Uuid> m_itemUuid;
};

} // namespace entity_resource_tree
} // namespace nx::vms::client::desktop
