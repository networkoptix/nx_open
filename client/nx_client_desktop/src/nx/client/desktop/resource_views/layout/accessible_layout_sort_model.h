#pragma once

#include <QtCore/QSortFilterProxyModel>

#include <nx/client/desktop/resource_views/node_view/sort/node_view_base_sort_model.h>

namespace nx {
namespace client {
namespace desktop {

class AccessibleLayoutSortModel: public NodeViewBaseSortModel
{
    Q_OBJECT
    using base_type = NodeViewBaseSortModel;

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

