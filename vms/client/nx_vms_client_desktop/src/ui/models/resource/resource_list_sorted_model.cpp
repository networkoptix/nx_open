#include "resource_list_sorted_model.h"
#include <nx/vms/client/desktop/resources/search_helper.h>

using namespace nx::vms::client::desktop;

QnResourceListSortedModel::QnResourceListSortedModel(QObject* parent):
    base_type(parent)
{
    setDynamicSortFilter(true);
    setSortRole(Qt::DisplayRole);
    setSortCaseSensitivity(Qt::CaseInsensitive);
}

QnResourceListSortedModel::~QnResourceListSortedModel()
{
}

bool QnResourceListSortedModel::lessThan(const QModelIndex& left, const QModelIndex& right) const
{
    return resourceLessThan(left, right);
}

bool QnResourceListSortedModel::filterAcceptsRow(
    int sourceRow,
    const QModelIndex& sourceParent) const
{
    if (filterRegExp().patternSyntax() == QRegExp::FixedString
        && resources::search_helper::isSearchStringValid(filterRegExp().pattern()))
    {
        const auto sourceIndex = sourceModel()->index(sourceRow, filterKeyColumn(), sourceParent);
        if (const auto resource = getResource(sourceIndex))
            return resources::search_helper::matches(filterRegExp().pattern(), resource);
    }
    return base_type::filterAcceptsRow(sourceRow, sourceParent);
}
