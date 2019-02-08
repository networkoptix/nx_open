#include "node_view_state_reducer.h"

#include "../details/node/view_node.h"
#include "../details/node/view_node_helpers.h"
#include "../details/node/view_node_data_builder.h"

namespace nx::vms::client::desktop {
namespace node_view {

using namespace details;

details::NodeViewStatePatch NodeViewStateReducer::setNodeChecked(
    const details::NodePtr& node,
    const details::ColumnSet& columns,
    Qt::CheckState nodeCheckedState)
{
    if (!node || columns.empty())
        return NodeViewStatePatch();

    NodeViewStatePatch patch;
    ViewNodeData data;

    bool hasChangedData = false;
    for (const int column: columns)
    {
        if (!checkable(node, column) || checkedState(node, column) == nodeCheckedState)
            continue;

        ViewNodeDataBuilder(data).withCheckedState(column, nodeCheckedState);
        hasChangedData = true;
    }

    if (hasChangedData)
        patch.addChangeStep(node->path(), data);
    return patch;

}

NodeViewStatePatch NodeViewStateReducer::setNodeChecked(
    const details::NodeViewState& state,
    const ViewNodePath& path,
    const details::ColumnSet& columns,
    Qt::CheckState nodeCheckedState)
{
    return setNodeChecked(state.nodeByPath(path), columns, nodeCheckedState);
}

NodeViewStatePatch NodeViewStateReducer::setNodeExpandedPatch(
    const details::NodeViewState& state,
    const ViewNodePath& path,
    bool value)
{
    const auto node = state.nodeByPath(path);
    if (!node || expanded(node) == value)
        return NodeViewStatePatch();

    NodeViewStatePatch patch;
    patch.addChangeStep(path, ViewNodeDataBuilder().withExpanded(value));
    return patch;
}

} // namespace node_view
} // namespace nx::vms::client::desktop

