#pragma once

#include <QtCore/QSortFilterProxyModel>

namespace nx::vms::client::desktop {

class IoPortsSortModel: public QSortFilterProxyModel
{
public:
    using QSortFilterProxyModel::QSortFilterProxyModel; //< Forward constructors.

protected:
    virtual bool lessThan(const QModelIndex& left, const QModelIndex& right) const override;
};

} // namespace nx::vms::client::core
