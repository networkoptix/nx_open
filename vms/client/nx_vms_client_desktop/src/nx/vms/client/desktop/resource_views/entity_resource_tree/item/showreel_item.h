// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/utils/scoped_connections.h>
#include <nx/vms/api/data/showreel_data.h>
#include <nx/vms/client/desktop/resource_views/entity_item_model/item/abstract_item.h>

namespace nx::vms::common { class ShowreelManager; }

namespace nx::vms::client::desktop {
namespace entity_resource_tree {

class ShowreelItem: public entity_item_model::AbstractItem
{
    using base_type = entity_item_model::AbstractItem;

public:
    ShowreelItem(const common::ShowreelManager* showreelManager, const QnUuid& id);

    /**
     * Implements AbstractItem::data().
     * @param  role From set of roles: Qt::DisplayRole, Qt::ToolTipRole, Qt::EditRole,
     *     Qn::ResourceIconKeyRole, Qn::NodeTypeRole, core::UuidRole.
     * @returns Data relevant to the one of the roles listed above, a null QVariant otherwise.
     */
    virtual QVariant data(int role) const override;

    /**
     * Implements AbstractItem::flags(). The provided set can be considered as set
     * possible, but no necessarily accessible actions.
     * @returns combination of Qt::ItemIsEnabled, Qt::ItemIsSelectable, Qt::ItemIsEditable,
     *     Qt::ItemIsDragEnabled, Qt::ItemNeverHasChildren.
     */
    virtual Qt::ItemFlags flags() const override;

private:
    void onShowreelChanged(const nx::vms::api::ShowreelData& showreel);

    nx::vms::api::ShowreelData m_showreel;
    nx::utils::ScopedConnections m_connectionsGuard;
};

} // namespace entity_resource_tree
} // namespace nx::vms::client::desktop
