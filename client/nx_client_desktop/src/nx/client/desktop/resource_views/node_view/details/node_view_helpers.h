#pragma once

#include <QtCore/QAbstractItemModel>

namespace nx {
namespace client {
namespace desktop {
namespace details {

const QAbstractItemModel* getSourceModel(const QAbstractItemModel* model);
QModelIndex getLeafIndex(const QModelIndex& index);
QModelIndex getRootIndex(
    const QModelIndex& leafIndex,
    const QAbstractItemModel* rootModel);

} // namespace details
} // namespace desktop
} // namespace client
} // namespace nx
