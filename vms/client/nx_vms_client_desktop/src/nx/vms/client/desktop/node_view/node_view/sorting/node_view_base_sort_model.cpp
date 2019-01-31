#include "node_view_base_sort_model.h"

namespace nx::vms::client::desktop {
namespace node_view {

struct NodeViewBaseSortModel::Private
{
    QString filter;
    NodeViewBaseSortModel::FilterScope scope;
};

NodeViewBaseSortModel::NodeViewBaseSortModel(QObject* parent):
    base_type(parent),
    d(new Private())
{
}

NodeViewBaseSortModel::~NodeViewBaseSortModel()
{
}

void NodeViewBaseSortModel::setFilter(const QString& filter, FilterScope scope)
{
    d->filter = filter;
    d->scope = scope;
    invalidateFilter();
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

bool NodeViewBaseSortModel::filterAcceptsRow(
    int sourceRow,
    const QModelIndex &sourceParent) const
{
    if (d->filter.isEmpty())
        return true;

    const auto source = sourceModel();
    if (!source)
        return true;

    const auto sourceColumnCount = source->columnCount(sourceParent);
    for (int column = 0; column != sourceColumnCount; ++column)
    {
        const auto index = source->index(sourceRow, column, sourceParent);
        if (d->scope == AnyNodeFilterScope || source->rowCount(index) == 0)
        {
            const auto text = index.data().toString();
            if (text.contains(d->filter, Qt::CaseInsensitive))
                return true;
        }

        const auto childrenCount = source->rowCount(index);
        for (int i = 0; i != childrenCount; ++i)
        {
            if (filterAcceptsRow(i, index))
                return true;
        }
    }
    return false;
}


} // namespace node_view
} // namespace nx::vms::client::desktop
