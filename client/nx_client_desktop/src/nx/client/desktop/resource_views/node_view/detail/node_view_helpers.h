#pragma once

#include <QtCore/QAbstractItemModel>

namespace nx {
namespace client {
namespace desktop {
namespace detail {

const QAbstractItemModel* getSourceModel(const QAbstractItemModel* model);
QModelIndex getSourceIndex(const QModelIndex& index, const QAbstractItemModel* model);

} // namespace detail
} // namespace desktop
} // namespace client
} // namespace nx
