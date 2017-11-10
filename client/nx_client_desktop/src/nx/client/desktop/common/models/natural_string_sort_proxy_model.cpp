#include "natural_string_sort_proxy_model.h"

#include <nx/utils/string.h>

namespace nx {
namespace client {
namespace desktop {

NaturalStringSortProxyModel::NaturalStringSortProxyModel(QObject* parent):
    base_type(parent)
{
    setCustomLessThan(
        [this](const QModelIndex& left, const QModelIndex& right) -> bool
        {
            return nx::utils::naturalStringCompare(
                left.data(Qt::DisplayRole).toString(),
                right.data(Qt::DisplayRole).toString(),
                sortCaseSensitivity()) < 0;
        });
}

} // namespace desktop
} // namespace client
} // namespace nx
