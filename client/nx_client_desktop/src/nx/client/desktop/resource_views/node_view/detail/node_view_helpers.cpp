#include "node_view_helpers.h"

#include <QtCore/QAbstractProxyModel>

namespace nx {
namespace client {
namespace desktop {
namespace detail {

const QAbstractItemModel* getSourceModel(const QAbstractItemModel* model)
{
    const auto proxyModel = qobject_cast<const QAbstractProxyModel*>(model);
    return proxyModel ? getSourceModel(proxyModel) : model;
}

QModelIndex getSourceIndex(const QModelIndex& index, const QAbstractItemModel* model)
{
    const auto proxyModel = qobject_cast<const QAbstractProxyModel*>(model);
    return proxyModel
        ? getSourceIndex(proxyModel->mapToSource(index), proxyModel)
        : index;
}

} // namespace detail
} // namespace desktop
} // namespace client
} // namespace nx

