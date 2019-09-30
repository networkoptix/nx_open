#include "node_view_numeric_sort_model.h"

namespace nx::vms::client::desktop {
namespace node_view {

NodeViewNumericSortModel::NodeViewNumericSortModel(QObject* parent):
    base_type(parent)
{
    m_collator.setNumericMode(true);
}

NodeViewNumericSortModel::~NodeViewNumericSortModel()
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
        m_collator.setCaseSensitivity(sortCaseSensitivity());
        return m_collator.compare(leftData.value<QString>(), rightData.value<QString>()) < 0;
    }
    return base_type::lessThan(sourceLeft, sourceRight);
}

bool NodeViewNumericSortModel::numericMode() const
{
    return m_collator.numericMode();
}

void NodeViewNumericSortModel::setNumericMode(bool isNumericMode)
{
    if (numericMode() == isNumericMode)
        return;

    m_collator.setNumericMode(isNumericMode);
    sort(sortColumn(), sortOrder());
}

} // namespace node_view
} // namespace nx::vms::client::desktop
