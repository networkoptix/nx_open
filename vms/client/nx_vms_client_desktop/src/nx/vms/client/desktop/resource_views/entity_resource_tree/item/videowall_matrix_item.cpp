// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "videowall_matrix_item.h"

#include <QtCore/QVariant>

#include <nx/vms/client/desktop/style/resource_icon_cache.h>
#include <core/resource/videowall_resource.h>
#include <client/client_globals.h>
#include <ui/help/help_topics.h>

namespace nx::vms::client::desktop {
namespace entity_resource_tree {

VideoWallMatrixItem::VideoWallMatrixItem(
    const QnVideoWallResourcePtr& videoWall,
    const QnUuid& matrixId)
    :
    base_type(),
    m_matrix(videoWall->matrices()->getItem(matrixId))
{
    m_connectionsGuard.add(videoWall->connect(videoWall.get(), &QnVideoWallResource::matrixChanged,
        [this](const QnVideoWallResourcePtr&, const QnVideoWallMatrix& matrix)
        {
            onMatrixChanged(matrix);
        }));
}

QVariant VideoWallMatrixItem::data(int role) const
{
    switch (role)
    {
        case Qt::DisplayRole:
        case Qt::ToolTipRole:
        case Qt::EditRole:
            return m_matrix.name;

        case Qn::ResourceIconKeyRole:
            return QVariant::fromValue<int>(QnResourceIconCache::VideoWallMatrix);

        case Qn::NodeTypeRole:
            return QVariant::fromValue(ResourceTree::NodeType::videoWallMatrix);

        case Qn::UuidRole:
        case Qn::ItemUuidRole:
            return QVariant::fromValue(m_matrix.uuid);

        case Qn::HelpTopicIdRole:
            return QVariant::fromValue<int>(Qn::Videowall_Management_Help);
    }

    return QVariant();
}

Qt::ItemFlags VideoWallMatrixItem::flags() const
{
    return {Qt::ItemIsEnabled, Qt::ItemIsSelectable, Qt::ItemIsDropEnabled,
        Qt::ItemIsEditable, Qt::ItemNeverHasChildren};
}

void VideoWallMatrixItem::onMatrixChanged(const QnVideoWallMatrix& matrix)
{
    if (matrix.uuid != m_matrix.uuid)
        return;

    const bool nameChanged = matrix.name != m_matrix.name;
    m_matrix = matrix;

    if (nameChanged)
        notifyDataChanged({Qt::DisplayRole, Qt::ToolTipRole, Qt::EditRole});
}

} // namespace entity_resource_tree
} // namespace nx::vms::client::desktop
