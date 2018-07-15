#pragma once

#include <QtCore/QSortFilterProxyModel>

namespace nx {
namespace client {
namespace desktop {

class NodeViewGroupSortingModel: public QSortFilterProxyModel
{
    using base_type = QSortFilterProxyModel;

public:
    NodeViewGroupSortingModel(QObject* parent = nullptr);

private:
    virtual bool lessThan(
        const QModelIndex& sourceLeft,
        const QModelIndex& sourceRight) const override;
};

} // namespace desktop
} // namespace client
} // namespace nx
