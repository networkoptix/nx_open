#include "node_view_state_reducer.h"

#include <nx/client/desktop/resource_views/node_view/node/view_node_data_builder.h>

namespace nx {
namespace client {
namespace desktop {
namespace details {

NodeViewStatePatch NodeViewStateReducer::setNodeChecked(
    const ViewNodePath& path,
    int column,
    Qt::CheckState state)
{
    NodeViewStatePatch patch;
    patch.changedData.push_back({path, ViewNodeDataBuilder().withCheckedState(column, state)});
    return patch;
}

NodeViewStatePatch NodeViewStateReducer::setNodeExpanded(
    const ViewNodePath& path,
    bool expanded)
{
    NodeViewStatePatch patch;
    patch.changedData.push_back({path, ViewNodeDataBuilder().withExpanded(expanded)});
    return patch;
}

} // namespace details
} // namespace desktop
} // namespace client
} // namespace nx

