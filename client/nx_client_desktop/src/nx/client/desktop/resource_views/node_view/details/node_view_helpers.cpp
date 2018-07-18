#include "node_view_helpers.h"

#include <QtCore/QAbstractProxyModel>

#include <nx/utils/log/log.h>

namespace nx {
namespace client {
namespace desktop {
namespace details {

const QAbstractItemModel* getSourceModel(const QAbstractItemModel* model)
{
    const auto proxyModel = qobject_cast<const QAbstractProxyModel*>(model);
    return proxyModel ? getSourceModel(proxyModel->sourceModel()) : model;
}

QModelIndex getLeafIndex(const QModelIndex& rootIndex, const QAbstractItemModel* rootModel)
{
    NX_EXPECT(rootIndex.model() == rootModel, "Invalid model!");
    const auto proxyModel = qobject_cast<const QAbstractProxyModel*>(rootModel);
    return proxyModel
        ? getLeafIndex(proxyModel->mapToSource(rootIndex), proxyModel->sourceModel())
        : rootIndex;
}

QModelIndex getRootIndex(
    int leafRow,
    int leafColumn,
    const QModelIndex& leafParent,
    const QAbstractItemModel* rootModel)
{
    const auto proxyModel = qobject_cast<const QAbstractProxyModel*>(rootModel);
    if (!proxyModel)
        return rootModel->index(leafRow, leafColumn, leafParent);

    const auto sourceIndex = getRootIndex(
        leafRow, leafColumn, leafParent, proxyModel->sourceModel());
    return proxyModel->mapFromSource(sourceIndex);
}

} // namespace detail
} // namespace desktop
} // namespace client
} // namespace nx

