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

QModelIndex getSourceIndex(const QModelIndex& index, const QAbstractItemModel* model)
{
    NX_EXPECT(index.model() == model, "Invalid model!");
    const auto proxyModel = qobject_cast<const QAbstractProxyModel*>(model);
    return proxyModel
        ? getSourceIndex(proxyModel->mapToSource(index), proxyModel->sourceModel())
        : index;
}

} // namespace detail
} // namespace desktop
} // namespace client
} // namespace nx

