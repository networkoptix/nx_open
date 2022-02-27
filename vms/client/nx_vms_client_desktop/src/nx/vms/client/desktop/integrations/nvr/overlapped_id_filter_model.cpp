// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "overlapped_id_filter_model.h"

namespace nx::vms::client::desktop::integrations {

OverlappedIdFilterModel::OverlappedIdFilterModel(QObject* parent):
    QnSortFilterListModel(parent)
{
}

bool OverlappedIdFilterModel::filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const
{
    if (filterWildcard().isEmpty())
        return true;

    const QModelIndex sourceIndex = sourceModel()->index(sourceRow, 0, sourceParent);
    //< id is an integer number or "Latest" in localization.
    QVariant id = sourceModel()->data(sourceIndex, Qt::DisplayRole);
    // No actual regexp here. Simple search for simple values range.
    return id.toString().toLower().contains(filterWildcard().toLower());
}

} // namespace nx::vms::client::desktop::integrations
