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

    virtual QModelIndex mapToSource(const QModelIndex &proxyIndex) const;
    virtual QModelIndex mapFromSource(const QModelIndex &sourceIndex) const;

protected:
    virtual bool lessThan(
        const QModelIndex& sourceLeft,
        const QModelIndex& sourceRight) const override;

    virtual bool filterAcceptsRow(int source_row, const QModelIndex &source_parent) const;

};

} // namespace desktop
} // namespace client
} // namespace nx

