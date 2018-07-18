#include "node_view_helpers.h"

#include <QtCore/QAbstractProxyModel>

#include <nx/utils/log/log.h>

namespace nx {
namespace client {
namespace desktop {
namespace detail {

const QAbstractItemModel* getSourceModel(const QAbstractItemModel* model)
{
    const auto proxyModel = qobject_cast<const QAbstractProxyModel*>(model);
    return proxyModel ? getSourceModel(proxyModel->sourceModel()) : model;
}

QModelIndex getSourceIndex(const QModelIndex& topIndex, const QAbstractItemModel* topModel)
{
    NX_EXPECT(topIndex.model() == topModel, "Invalid model!");
    const auto proxyModel = qobject_cast<const QAbstractProxyModel*>(topModel);
    return proxyModel
        ? getSourceIndex(proxyModel->mapToSource(topIndex), proxyModel->sourceModel())
        : topIndex;
}

QModelIndex getTopIndex(
    int sourceRow,
    int sourceColumn,
    const QModelIndex& sourceParent,
    const QAbstractItemModel* topModel)
{
    const auto proxyModel = qobject_cast<const QAbstractProxyModel*>(topModel);
    if (!proxyModel)
        return topModel->index(sourceRow, sourceColumn, sourceParent);

    const auto sourceIndex = getTopIndex(
        sourceRow, sourceColumn, sourceParent, proxyModel->sourceModel());
    return proxyModel->mapFromSource(sourceIndex);
}

} // namespace detail
} // namespace desktop
} // namespace client
} // namespace nx

