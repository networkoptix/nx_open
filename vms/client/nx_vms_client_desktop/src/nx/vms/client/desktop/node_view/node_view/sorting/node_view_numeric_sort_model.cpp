#include "node_view_numeric_sort_model.h"
#include <QCollator>

namespace nx::vms::client::desktop {
namespace node_view {

struct NodeViewNumericSortModel::Private
{
    NodeViewNumericSortModel::NumericMode numericMode = Lexicographic;
};

NodeViewNumericSortModel::NodeViewNumericSortModel(QObject* parent):
    base_type(parent),
    d(new Private())
{
}

bool NodeViewNumericSortModel::lessThan(
    const QModelIndex& sourceLeft,
    const QModelIndex& sourceRight) const
{
    auto leftData = sourceLeft.data(sortRole());
    auto rightData = sourceRight.data(sortRole());
    if (leftData.type() == QVariant::String && rightData.type() == QVariant::String)
    {
        QCollator collator;
        collator.setCaseSensitivity(sortCaseSensitivity());
        switch (d->numericMode)
        {
            case Lexicographic:
                collator.setNumericMode(false);
                break;
            case Alphanumeric:
                collator.setNumericMode(true);
                break;
            default:
                NX_ASSERT(false);
                break;
        }
        return collator.compare(leftData.value<QString>(), rightData.value<QString>()) < 0;
    }
    return base_type::lessThan(sourceLeft, sourceRight);
}

void NodeViewNumericSortModel::setNumericMode(NumericMode mode)
{
    if (d->numericMode == mode)
        return;

    d->numericMode = mode;
    sort(sortColumn(), sortOrder());
}

NodeViewNumericSortModel::NumericMode NodeViewNumericSortModel::getNumericMode() const
{
    return d->numericMode;
}

} // namespace node_view
} // namespace nx::vms::client::desktop
