// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "lookup_list_entries_sort_filter_proxy_model.h"

#include <QtQml/QtQml>

#include <core/resource/camera_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/resource/search_helper.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/rules/ini.h>

#include "lookup_list_entries_model.h"

namespace nx::vms::client::desktop {

LookupListEntriesSortFilterProxyModel::LookupListEntriesSortFilterProxyModel(QObject* parent):
    QSortFilterProxyModel{parent}
{
    setDynamicSortFilter(true);
    setFilterCaseSensitivity(Qt::CaseInsensitive);
    setFilterKeyColumn(-1);
}

bool LookupListEntriesSortFilterProxyModel::filterAcceptsRow(
    int sourceRow, const QModelIndex& sourceParent) const
{
    const auto source = sourceModel();
    if (!source)
        return true;

    const auto filterRegExp = filterRegularExpression();
    if (filterRegExp.pattern().isEmpty())
        return true;

    for (int column = 0; column < sourceModel()->rowCount(); column++)
    {
        QModelIndex sourceIndex = sourceModel()->index(sourceRow, column, sourceParent);
        if (sourceIndex.data().toString().contains(filterRegExp))
            return true;
    }

    return false;
}

} // namespace nx::vms::client::desktop
