// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "invalid_resource_filter_model.h"

#include <nx/vms/client/desktop/resource_dialogs/resource_dialogs_constants.h>
#include <nx/vms/client/desktop/resource_views/data/resource_tree_globals.h>

namespace nx::vms::client::desktop {

InvalidResourceFilterModel::InvalidResourceFilterModel(QObject* parent):
    base_type(parent)
{
    setDynamicSortFilter(true);
    setRecursiveFilteringEnabled(true);
}

void InvalidResourceFilterModel::setShowInvalidResources(bool show)
{
    if (m_showInvalidResources == show)
        return;

    m_showInvalidResources = show;
    invalidateFilter();
}

bool InvalidResourceFilterModel::showInvalidResources() const
{
    return m_showInvalidResources;
}

bool InvalidResourceFilterModel::filterAcceptsRow(
    int sourceRow,
    const QModelIndex& sourceParent) const
{
    if (showInvalidResources())
        return true;

    const auto sourceIndex = sourceModel()->index(sourceRow, 0, sourceParent);

    if (sourceIndex.data(ResourceDialogItemRole::IsPinnedItemRole).toBool())
        return true;

    const auto isValidResourceData = sourceIndex.data(ResourceDialogItemRole::IsValidResourceRole);
    if (isValidResourceData.isNull())
        return false;

    return isValidResourceData.toBool();
}

} // namespace nx::vms::client::desktop
