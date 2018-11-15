#include "io_ports_sort_model.h"

#include <ui/models/ioports_view_model.h>

#include <nx/utils/string.h>

namespace nx::vms::client::desktop {

bool IoPortsSortModel::lessThan(const QModelIndex& left, const QModelIndex& right) const
{
    // Numbers have special sorting.
    if (left.column() == QnIOPortsViewModel::NumberColumn)
        return left.row() < right.row();

    // Durations are sorted by integer value.
    if (left.column() == QnIOPortsViewModel::DurationColumn)
    {
        const auto leftValue = left.data(Qt::EditRole).toInt();
        const auto rightValue = right.data(Qt::EditRole).toInt();

        /* Zero (unset) durations are always last: */
        const bool leftZero = (leftValue == 0);
        const bool rightZero = (rightValue == 0);
        if (leftZero != rightZero)
            return rightZero;

        return leftValue < rightValue;
    }

    // Other columns are sorted by string value.
    const auto leftStr = left.data(Qt::DisplayRole).toString();
    const auto rightStr = right.data(Qt::DisplayRole).toString();

    // Empty strings are always last.
    if (leftStr.isEmpty() != rightStr.isEmpty())
        return rightStr.isEmpty();

    return nx::utils::naturalStringCompare(leftStr, rightStr, Qt::CaseInsensitive) < 0;
}

} // namespace nx::vms::client::core

