#pragma once

#include <QtCore/QAbstractItemModel>

namespace nx {
namespace client {
namespace desktop {
namespace detail {

const QAbstractItemModel* getSourceModel(const QAbstractItemModel* model);
QModelIndex getSourceIndex(const QModelIndex& topIndex, const QAbstractItemModel* topModel);
QModelIndex getTopIndex(
    int sourceRow,
    int sourceColumn,
    const QModelIndex& sourceParent,
    const QAbstractItemModel* topModel);

} // namespace detail
} // namespace desktop
} // namespace client
} // namespace nx
