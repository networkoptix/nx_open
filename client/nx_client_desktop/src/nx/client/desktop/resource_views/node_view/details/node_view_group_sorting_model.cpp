#include "node_view_group_sorting_model.h"

#include <nx/client/desktop/resource_views/node_view/node/view_node_helpers.h>

namespace nx {
namespace client {
namespace desktop {
namespace details {

NodeViewGroupSortingModel::NodeViewGroupSortingModel(QObject* parent):
    base_type(parent)
{
}

bool NodeViewGroupSortingModel::lessThan(
    const QModelIndex& sourceLeft,
    const QModelIndex& sourceRight) const
{
    const int leftGroup = helpers::siblingGroup(sourceLeft);
    const int rightGroup = helpers::siblingGroup(sourceRight);
    return leftGroup == rightGroup
        ? nextLessThan(sourceLeft, sourceRight)
        : leftGroup > rightGroup;
}

} // namespace details
} // namespace desktop
} // namespace client
} // namespace nx

