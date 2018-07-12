#pragma once

#include <QtCore/QSortFilterProxyModel>

namespace nx {
namespace client {
namespace desktop {

class AccessibleLayoutSortModel: public QSortFilterProxyModel
{
    Q_OBJECT
    using base_type = QSortFilterProxyModel;

public:
    AccessibleLayoutSortModel(QObject* parent = nullptr);

protected:
    virtual bool lessThan(
        const QModelIndex& sourceLeft,
        const QModelIndex& sourceRight) const override;
};

} // namespace desktop
} // namespace client
} // namespace nx

