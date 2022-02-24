// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "filtered_resource_proxy_model.h"

#include <client/client_globals.h>
#include <core/resource/resource.h>
#include <nx/vms/client/desktop/resources/search_helper.h>
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
    const auto filterString = filterRegularExpression().pattern();
    if (filterString.trimmed().isEmpty())
        return true;

    const auto sourceIndex = sourceModel()->index(sourceRow, 0, sourceParent);
    const auto resourceData = sourceIndex.data(Qn::ResourceRole);
    if (resourceData.isValid())
    {
        const auto resource = resourceData.value<QnResourcePtr>();
        return resources::search_helper::matches(filterString, resource);
    }
    return sourceIndex.data().toString().contains(filterString, filterCaseSensitivity());
}

} // namespace nx::vms::client::desktop
