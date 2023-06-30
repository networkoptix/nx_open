// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "filtered_resource_proxy_model.h"

#include <client/client_globals.h>
#include <core/resource/resource.h>
#include <nx/vms/client/desktop/resource/search_helper.h>
#include <nx/vms/client/desktop/resource_views/data/resource_tree_globals.h>

namespace nx::vms::client::desktop {

FilteredResourceProxyModel::FilteredResourceProxyModel(QObject* parent /*= nullptr*/):
    base_type(parent)
{
    setDynamicSortFilter(true);
    setFilterCaseSensitivity(Qt::CaseInsensitive);
    setRecursiveFilteringEnabled(true);
}

bool FilteredResourceProxyModel::filterAcceptsRow(
    int sourceRow,
    const QModelIndex& sourceParent) const
{
    // Make a filter string from escaped fixed string regexp pattern (eg "a\ b" -> "a b") so that
    // the function search_helper::matches() can split the filter string into words.
    static const QRegularExpression escapedRx("\\\\(.)");
    const auto filterString = filterRegularExpression().pattern().replace(escapedRx, "\\1");
    if (filterString.trimmed().isEmpty())
        return true;

    const auto sourceIndex = sourceModel()->index(sourceRow, 0, sourceParent);
    const auto resourceData = sourceIndex.data(core::ResourceRole);
    if (resourceData.isValid())
    {
        const auto resource = resourceData.value<QnResourcePtr>();
        return resources::search_helper::matches(filterString, resource);
    }
    return filterRegularExpression().match(sourceIndex.data().toString()).hasMatch();
}

} // namespace nx::vms::client::desktop
