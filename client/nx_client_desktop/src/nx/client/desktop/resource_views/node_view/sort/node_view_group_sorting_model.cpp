#include "node_view_group_sorting_model.h"

#include <nx/client/desktop/resource_views/node_view/node_view_model.h>
#include <nx/client/desktop/resource_views/node_view/node_view_constants.h>
#include <nx/client/desktop/resource_views/node_view/nodes/view_node.h>
#include <nx/client/desktop/resource_views/node_view/detail/node_view_helpers.h>

namespace {

using namespace nx::client::desktop;

int getNodeSiblingGroup(const QModelIndex& sourceIndex, const QAbstractItemModel* model)
{
    const auto mapped = detail::getSourceIndex(sourceIndex, model);
    const auto node = NodeViewModel::nodeFromIndex(mapped);
    const auto data = node->data(node_view::nameColumn, node_view::siblingGroupRole);
    return data.isValid() ? data.toInt() : 0;
}

} // namespace

namespace nx {
namespace client {
namespace desktop {

NodeViewGroupSortingModel::NodeViewGroupSortingModel(QObject* parent):
    base_type(parent)
{
}

bool NodeViewGroupSortingModel::lessThan(
    const QModelIndex& sourceLeft,
    const QModelIndex& sourceRight) const
{
    const int leftGroup = getNodeSiblingGroup(sourceLeft, sourceModel());
    const int rightGroup = getNodeSiblingGroup(sourceRight, sourceModel());
    return leftGroup == rightGroup
        ? nextLessThan(sourceLeft, sourceRight)
        : leftGroup > rightGroup;
}

} // namespace desktop
} // namespace client
} // namespace nx

