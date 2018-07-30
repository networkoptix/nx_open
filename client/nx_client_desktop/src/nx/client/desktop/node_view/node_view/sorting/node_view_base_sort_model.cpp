#include "node_view_base_sort_model.h"

namespace nx {
namespace client {
namespace desktop {
namespace node_view {

NodeViewBaseSortModel::NodeViewBaseSortModel(QObject* parent):
    base_type(parent)
{
}

void NodeViewBaseSortModel::setSourceModel(QAbstractItemModel* model)
{
    const auto proxySource = qobject_cast<NodeViewBaseSortModel*>(sourceModel());
    if (proxySource)
        proxySource->setSourceModel(model);
    else
        base_type::setSourceModel(model);
}

bool NodeViewBaseSortModel::nextLessThan(
    const QModelIndex& sourceLeft,
    const QModelIndex& sourceRight) const
{
    const auto proxySource = qobject_cast<const NodeViewBaseSortModel*>(sourceModel());
    if (!proxySource)
        return base_type::lessThan(sourceLeft, sourceRight);

    const auto proxySourceLeft = proxySource->mapToSource(sourceLeft);
    const auto proxySourceRight = proxySource->mapToSource(sourceRight);
    return proxySource->lessThan(proxySourceLeft, proxySourceRight);
}

} // namespace node_view
} // namespace desktop
} // namespace client
} // namespace nx
