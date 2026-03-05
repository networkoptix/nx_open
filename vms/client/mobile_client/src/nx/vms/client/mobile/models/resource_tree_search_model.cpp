// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "resource_tree_search_model.h"

#include <core/resource/camera_resource.h>
#include <nx/vms/client/core/client_core_globals.h>
#include <nx/vms/client/core/resource_views/data/resource_tree_globals.h>
#include <nx/vms/client/core/resource_views/entity_resource_tree/resource_grouping/resource_grouping.h>

namespace nx::vms::client::mobile {

ResourceTreeSearchModel::ResourceTreeSearchModel(QObject* parent):
    base_type(parent)
{
    setDynamicSortFilter(true);
    setRecursiveFilteringEnabled(true);
}

ResourceTreeSearchModel::~ResourceTreeSearchModel()
{
}

bool ResourceTreeSearchModel::filterAcceptsRow(
    int sourceRow,
    const QModelIndex& sourceParent) const
{
    using namespace core::entity_resource_tree;

    if (!filterRegularExpression().pattern().isEmpty())
    {
        const auto sourceIndex = sourceModel()->index(sourceRow, 0, sourceParent);
        const auto nodeTypeData = sourceIndex.data(core::NodeTypeRole);
        if (!nodeTypeData.isNull())
        {
            const auto nodeType = nodeTypeData.value<core::ResourceTree::NodeType>();
            if (nodeType == core::ResourceTree::NodeType::camerasAndDevices
                || nodeType == core::ResourceTree::NodeType::layouts)
            {
                return false;
            }
        }

        const auto resource = sourceIndex.data(core::ResourceRole).value<QnResourcePtr>();
        if (const auto camera = resource.objectCast<QnVirtualCameraResource>())
        {
            if (camera->isMultiSensorCamera()
                && filterRegularExpression().match(camera->getUserDefinedGroupName()).hasMatch())
            {
                return true;
            }
        }

        if (resource)
        {
            const auto customGroupIdData = sourceIndex.data(core::ResourceTreeCustomGroupIdRole);
            if (!customGroupIdData.isNull())
            {
                const auto customGroupId = customGroupIdData.toString();
                for (int i = 0; i < resource_grouping::compositeIdDimension(customGroupId); ++i)
                {
                    const auto subId = resource_grouping::extractSubId(customGroupId, i);
                    if (filterRegularExpression().match(subId).hasMatch())
                        return true;
                }
            }
        }
    }

    return base_type::filterAcceptsRow(sourceRow, sourceParent);
}

} // namespace nx::vms::client::mobile
