#pragma once

#include <QtCore/QSortFilterProxyModel>

namespace nx {
namespace client {
namespace desktop {

class IoPortsSortModel: public QSortFilterProxyModel
{
public:
    using QSortFilterProxyModel::QSortFilterProxyModel; //< Forward constructors.

protected:
    virtual bool lessThan(const QModelIndex& left, const QModelIndex& right) const override;
};

} // namespace core
} // namespace client
} // namespace nx
