// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <core/resource/resource_fwd.h>
#include <core/resource/videowall_item.h>
#include <nx/utils/scoped_connections.h>
#include <nx/vms/client/desktop/resource_views/entity_item_model/item/abstract_item.h>

namespace nx::vms::client::desktop {
namespace entity_resource_tree {

class VideoWallScreenItem: public entity_item_model::AbstractItem
{
    using base_type = entity_item_model::AbstractItem;

public:
    VideoWallScreenItem(const QnVideoWallResourcePtr& videoWall, const nx::Uuid& screenId);

    /**
     * Implements AbstractItem::data().
     * @todo Qn::UuidRole is alias for Qn::ItemUuidRole and will be removed.
     * @param  role From set of roles: Qt::DisplayRole, Qt::ToolTipRole, Qt::EditRole,
     *     Qn::ResourceIconKeyRole, Qn::NodeTypeRole, Qn::UuidRole, Qn::ItemUuidRole,
     *     Qn::HelpTopicIdRole.
     * @returns Data relevant to the one of the roles listed above, a null QVariant otherwise.
     */
    virtual QVariant data(int role) const override;

    /**
     * Implements AbstractItem::flags(). The provided set can be considered as set possible,
     * but no necessarily accessible actions. May be filtered upstream.
     * @returns combination of Qt::ItemIsEnabled, Qt::ItemIsSelectable, Qt::ItemIsEditable,
     *     Qt::ItemIsDragEnabled, Qt::ItemIsDropEnabled, Qt::ItemNeverHasChildren.
     */
    virtual Qt::ItemFlags flags() const override;

private:
    void onScreenChanged(const QnVideoWallItem& screen, const QnVideoWallItem& oldScreen);

    QnVideoWallResourcePtr m_videoWall;
    QnVideoWallItem m_screen;
    nx::utils::ScopedConnections m_connectionsGuard;
};

} // namespace entity_resource_tree
} // namespace nx::vms::client::desktop
