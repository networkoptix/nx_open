#pragma once

#include "node_view_base_sort_model.h"

namespace nx::vms::client::desktop {
namespace node_view {

class NodeViewGroupSortingModel: public NodeViewBaseSortModel
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

} // namespace node_view
} // namespace nx::vms::client::desktop
