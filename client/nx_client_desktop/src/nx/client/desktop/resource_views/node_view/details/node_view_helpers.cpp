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

QModelIndex getLeafIndex(const QModelIndex& index)
{
    const auto proxyModel = qobject_cast<const QAbstractProxyModel*>(index.model());
    return proxyModel ? getLeafIndex(proxyModel->mapToSource(index)) : index;
}

QModelIndex getRootIndex(
    const QModelIndex& leafIndex,
    const QAbstractItemModel* rootModel)
{
    const auto proxyModel = qobject_cast<const QAbstractProxyModel*>(rootModel);
    if (!proxyModel)
        return rootModel->index(leafIndex.row(), leafIndex.column(), leafIndex.parent());

    const auto sourceIndex = getRootIndex(leafIndex, proxyModel->sourceModel());
    return proxyModel->mapFromSource(sourceIndex);
}

} // namespace detail
} // namespace desktop
} // namespace client
} // namespace nx

