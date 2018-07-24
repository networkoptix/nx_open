#pragma once

#include <nx/client/desktop/resource_views/node_view/details/node_view_base_sort_model.h>

namespace nx {
namespace client {
namespace desktop {
namespace details {

class NodeViewGroupSortingModel: public details::NodeViewBaseSortModel
{
    Q_OBJECT
    using base_type = NodeViewBaseSortModel;

public:
    NodeViewGroupSortingModel(QObject* parent = nullptr);

private:
    virtual bool lessThan(
        const QModelIndex& sourceLeft,
        const QModelIndex& sourceRight) const override;
};

} // namespace details
} // namespace desktop
} // namespace client
} // namespace nx
