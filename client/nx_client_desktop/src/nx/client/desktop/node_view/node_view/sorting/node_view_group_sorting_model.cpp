#include "node_view_group_sorting_model.h"

#include "../../details/node/view_node_helpers.h"

namespace nx {
namespace client {
namespace desktop {
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
    const int leftGroup = siblingGroup(sourceLeft);
    const int rightGroup = siblingGroup(sourceRight);
    return leftGroup == rightGroup
        ? nextLessThan(sourceLeft, sourceRight)
        : leftGroup > rightGroup;
}

} // namespace node_view
} // namespace desktop
} // namespace client
} // namespace nx

