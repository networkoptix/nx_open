// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <core/resource/resource_fwd.h>
#include <core/resource/videowall_matrix.h>
#include <nx/utils/scoped_connections.h>
#include <nx/vms/client/desktop/resource_views/entity_item_model/item/abstract_item.h>

namespace nx::vms::client::desktop {
namespace entity_resource_tree {

class VideoWallMatrixItem: public entity_item_model::AbstractItem
{
    using base_type = entity_item_model::AbstractItem;
public:
    VideoWallMatrixItem(const QnVideoWallResourcePtr& videoWall, const QnUuid& matrixId);

    /**
     * Implements AbstractItem::data().
     * @todo core::UuidRole is alias for Qn::ItemUuidRole and will be removed.
     * @param role From set of roles: Qt::DisplayRole, Qt::ToolTipRole, Qt::EditRole,
     *     Qn::ResourceIconKeyRole, Qn::NodeTypeRole, core::UuidRole, Qn::ItemUuidRole,
     *     Qn::HelpTopicIdRole.
     * @returns Data relevant to the one of the roles listed above, a null QVariant otherwise.
     */
    virtual QVariant data(int role) const override;

    /**
    * Implements AbstractItem::flags().
    * @returns combination of Qt::ItemIsEnabled, Qt::ItemIsSelectable, Qt::ItemIsEditable,
    *     Qt::ItemIsDragEnabled, Qt::ItemNeverHasChildren.
    */
    virtual Qt::ItemFlags flags() const override;

private:
    void onMatrixChanged(const QnVideoWallMatrix& matrix);

private:
    QnVideoWallMatrix m_matrix;
    nx::utils::ScopedConnections m_connectionsGuard;
};

} // namespace entity_resource_tree
} // namespace nx::vms::client::desktop
