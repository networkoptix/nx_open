#pragma once

#include <nx/client/desktop/resource_views/node_view/sort/node_view_base_sort_model.h>

namespace nx {
namespace client {
namespace desktop {

class NodeViewGroupSortingModel: public NodeViewBaseSortModel
{
    using base_type = NodeViewBaseSortModel;

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
