// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "resource_tree_search_model.h"

#include <nx/vms/client/core/client_core_globals.h>
#include <nx/vms/client/core/resource_views/data/resource_tree_globals.h>

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
    }

    return base_type::filterAcceptsRow(sourceRow, sourceParent);
}

} // namespace nx::vms::client::mobile
