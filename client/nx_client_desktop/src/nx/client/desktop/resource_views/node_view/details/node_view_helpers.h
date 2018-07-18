#pragma once

#include <QtCore/QAbstractItemModel>

namespace nx {
namespace client {
namespace desktop {
namespace details {

const QAbstractItemModel* getSourceModel(const QAbstractItemModel* model);
QModelIndex getLeafIndex(const QModelIndex& rootIndex, const QAbstractItemModel* rootModel);
QModelIndex getRootIndex(
    int leafRow,
    int leafColumn,
    const QModelIndex& leafParent,
    const QAbstractItemModel* rootModel);

} // namespace details
} // namespace desktop
} // namespace client
} // namespace nx
