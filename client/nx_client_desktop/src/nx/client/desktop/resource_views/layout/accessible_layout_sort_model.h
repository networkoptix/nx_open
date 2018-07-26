#pragma once

#include <QtCore/QSortFilterProxyModel>

#include <nx/client/desktop/resource_views/node_view/details/node_view_base_sort_model.h>

namespace nx {
namespace client {
namespace desktop {

// TODO: rename and move it to appropriate place
class AccessibleLayoutSortModel: public details::NodeViewBaseSortModel
{
    Q_OBJECT
    using base_type = NodeViewBaseSortModel;

public:
    AccessibleLayoutSortModel(int column, QObject* parent = nullptr);

    // TODO: move to base proxy of node view
    void setFilter(const QString& filter);

protected:
    virtual bool lessThan(
        const QModelIndex& sourceLeft,
        const QModelIndex& sourceRight) const override;

    virtual bool filterAcceptsRow(
        int sourceRow,
        const QModelIndex &sourceParent) const override;

private:
    const int m_column;
    QString m_filter;
};

} // namespace desktop
} // namespace client
} // namespace nx

