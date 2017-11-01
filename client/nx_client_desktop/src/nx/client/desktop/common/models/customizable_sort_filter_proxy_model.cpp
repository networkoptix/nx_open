#include "customizable_sort_filter_proxy_model.h"

namespace nx {
namespace client {
namespace desktop {

void CustomizableSortFilterProxyModel::setCustomLessThan(LessPredicate lessThan)
{
    m_lessThan = lessThan;
    sort(sortColumn(), sortOrder());
}

bool CustomizableSortFilterProxyModel::lessThan(
    const QModelIndex& sourceLeft,
    const QModelIndex& sourceRight) const
{
    return m_lessThan
        ? m_lessThan(sourceLeft, sourceRight)
        : base_type::lessThan(sourceLeft, sourceRight);
}

void CustomizableSortFilterProxyModel::setCustomFilterAcceptsRow(
    AcceptPredicate filterAcceptsRow)
{
    m_filterAcceptsRow = filterAcceptsRow;
    invalidateFilter();
}

void CustomizableSortFilterProxyModel::setCustomFilterAcceptsColumn(
    AcceptPredicate filterAcceptsColumn)
{
    m_filterAcceptsColumn = filterAcceptsColumn;
    invalidateFilter();
}

bool CustomizableSortFilterProxyModel::filterAcceptsRow(
    int sourceRow,
    const QModelIndex& sourceParent) const
{
    return m_filterAcceptsRow
        ? m_filterAcceptsRow(sourceRow, sourceParent)
        : base_type::filterAcceptsRow(sourceRow, sourceParent);
}

bool CustomizableSortFilterProxyModel::filterAcceptsColumn(
    int sourceColumn,
    const QModelIndex& sourceParent) const
{
    return m_filterAcceptsColumn
        ? m_filterAcceptsColumn(sourceColumn, sourceParent)
        : base_type::filterAcceptsColumn(sourceColumn, sourceParent);
}

bool CustomizableSortFilterProxyModel::baseFilterAcceptsRow(int sourceRow,
    const QModelIndex& sourceParent) const
{
    return base_type::filterAcceptsRow(sourceRow, sourceParent);
}

bool CustomizableSortFilterProxyModel::baseFilterAcceptsColumn(int sourceColumn,
    const QModelIndex& sourceParent) const
{
    return base_type::filterAcceptsColumn(sourceColumn, sourceParent);
}

} // namespace desktop
} // namespace client
} // namespace nx
