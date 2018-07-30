#pragma once

#include <QtCore/QSortFilterProxyModel>

namespace nx {
namespace client {
namespace desktop {
namespace node_view {

class NodeViewBaseSortModel: public QSortFilterProxyModel
{
    Q_OBJECT
    using base_type = QSortFilterProxyModel;

public:
    NodeViewBaseSortModel(QObject* parent = nullptr);

    virtual void setSourceModel(QAbstractItemModel* model) override;

protected:
    bool nextLessThan(
        const QModelIndex& sourceLeft,
        const QModelIndex& sourceRight) const;
};

} // namespace node_view
} // namespace desktop
} // namespace client
} // namespace nx
