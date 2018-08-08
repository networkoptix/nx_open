#include "node_view_state_reducer.h"

#include "../details/node/view_node_helpers.h"
#include "../details/node/view_node_data_builder.h"

namespace nx {
namespace client {
namespace desktop {
namespace node_view {

using namespace details;

NodeViewStatePatch NodeViewStateReducer::setNodeChecked(
    const details::NodeViewState& state,
    const ViewNodePath& path,
    const details::ColumnsSet& columns,
    Qt::CheckState nodeCheckedState)
{
    const auto node = state.nodeByPath(path);
    if (!node)
        return NodeViewStatePatch();

    NodeViewStatePatch patch;
    ViewNodeData data;
    for (const int column: columns)
    {
        if (checkable(node, column) && checkedState(node, column) != nodeCheckedState)
            ViewNodeDataBuilder(data).withCheckedState(column, nodeCheckedState);
    }

    patch.addChangeStep(path, data);
    return patch;
}

NodeViewStatePatch NodeViewStateReducer::setNodeExpandedPatch(
    const ViewNodePath& path,
    bool expanded)
{
    NodeViewStatePatch patch;
    patch.addChangeStep(path, ViewNodeDataBuilder().withExpanded(expanded));
    return patch;
}

} // namespace node_view
} // namespace desktop
} // namespace client
} // namespace nx

