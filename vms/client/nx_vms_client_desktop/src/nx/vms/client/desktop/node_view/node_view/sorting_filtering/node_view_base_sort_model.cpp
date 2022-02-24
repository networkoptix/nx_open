// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "node_view_base_sort_model.h"

#include <nx/vms/client/desktop/node_view/details/node/view_node_helper.h>

namespace nx::vms::client::desktop {
namespace node_view {

struct NodeViewBaseSortModel::Private
{
    bool filteringEnabled = true;
    QString filterString;
    FilterFunction filterFunction;
};

NodeViewBaseSortModel::NodeViewBaseSortModel(QObject* parent):
    base_type(parent),
    d(new Private())
{
}

NodeViewBaseSortModel::~NodeViewBaseSortModel()
{
}

bool NodeViewBaseSortModel::filteringEnabled() const
{
    return d->filteringEnabled;
}

void NodeViewBaseSortModel::setFilteringEnabled(bool enabled)
{
    if (d->filteringEnabled == enabled)
        return;

    d->filteringEnabled = enabled;
    invalidateFilter();
}

QString NodeViewBaseSortModel::filterString() const
{
    return d->filterString;
}

void NodeViewBaseSortModel::setFilterString(const QString& string)
{
    if (d->filterString == string)
        return;

    d->filterString = string;
    invalidateFilter();
}

void NodeViewBaseSortModel::setFilterFunction(FilterFunction filterFunction)
{
    d->filterFunction = filterFunction;
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
    const QModelIndex& sourceParent) const
{
    if (d->filterString.isEmpty() || !d->filteringEnabled)
        return true;

    const auto source = sourceModel();
    if (!source)
        return true;

    const auto sourceColumnCount = source->columnCount(sourceParent);
    for (int column = 0; column != sourceColumnCount; ++column)
    {
        const auto index = source->index(sourceRow, column, sourceParent);

        if (details::ViewNodeHelper(index).isSeparator() || details::ViewNodeHelper::isHeader(index))
            return false;

        if (source->rowCount(index) == 0)
        {
            if (d->filterFunction && d->filterFunction(index, d->filterString))
                return true;
            else if (index.data().toString().contains(d->filterString, Qt::CaseInsensitive))
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
