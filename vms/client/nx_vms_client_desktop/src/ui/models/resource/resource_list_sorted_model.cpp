// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "resource_list_sorted_model.h"
#include <nx/vms/client/desktop/resource/search_helper.h>
#include <ui/models/resource/resource_compare_helper.h>
#include <core/resource/resource.h>
#include <client/client_globals.h>

using namespace nx::vms::client::desktop;

QnResourceListSortedModel::QnResourceListSortedModel(QObject* parent):
    base_type(parent)
{
    setDynamicSortFilter(true);
    setSortRole(Qt::DisplayRole);
    setSortCaseSensitivity(Qt::CaseInsensitive);
    m_collator.setNumericMode(true);
    m_collator.setCaseSensitivity(sortCaseSensitivity());
}

QnResourceListSortedModel::~QnResourceListSortedModel()
{
}

bool QnResourceListSortedModel::lessThan(const QModelIndex& left, const QModelIndex& right) const
{
    return QnResourceCompareHelper::resourceLessThan(left, right, m_collator);
}

bool QnResourceListSortedModel::filterAcceptsRow(
    int sourceRow,
    const QModelIndex& sourceParent) const
{
    if (filterRegExp().patternSyntax() == QRegExp::FixedString
        && resources::search_helper::isSearchStringValid(filterRegExp().pattern()))
    {
        const auto sourceIndex = sourceModel()->index(sourceRow, filterKeyColumn(), sourceParent);
        if (const auto resource = sourceIndex.data(Qn::ResourceRole).value<QnResourcePtr>())
            return resources::search_helper::matches(filterRegExp().pattern(), resource);
    }
    return base_type::filterAcceptsRow(sourceRow, sourceParent);
}
