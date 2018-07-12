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

    void setFilter(const QString& filter);

protected:
    virtual bool lessThan(
        const QModelIndex& sourceLeft,
        const QModelIndex& sourceRight) const override;

    virtual bool filterAcceptsRow(
        int sourceRow,
        const QModelIndex &sourceParent) const override;

private:
    QString m_filter;
};

} // namespace desktop
} // namespace client
} // namespace nx

