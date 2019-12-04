#include "node_view_group_sorting_model.h"

#include "../../details/node/view_node_helper.h"

namespace nx::vms::client::desktop {
namespace node_view {

using namespace details;

NodeViewGroupSortingModel::NodeViewGroupSortingModel(QObject* parent):
    base_type(parent)
{
}

bool NodeViewGroupSortingModel::lessThan(
    const QModelIndex& sourceLeft,
    const QModelIndex& sourceRight) const
{
    const int leftGroupOrder = ViewNodeHelper(sourceLeft).groupSortOrder();
    const int rightGroupOrder = ViewNodeHelper(sourceRight).groupSortOrder();
    return leftGroupOrder == rightGroupOrder
        ? nextLessThan(sourceLeft, sourceRight)
        : leftGroupOrder < rightGroupOrder;
}

} // namespace node_view
} // namespace nx::vms::client::desktop

