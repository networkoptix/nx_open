// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "node_view_state_reducer.h"

#include "../details/node/view_node.h"
#include "../details/node/view_node_helper.h"
#include "../details/node/view_node_data_builder.h"

namespace nx::vms::client::desktop {
namespace node_view {

using namespace details;

details::NodeViewStatePatch NodeViewStateReducer::setNodeChecked(
    const details::NodePtr& node,
    const details::ColumnSet& columns,
    Qt::CheckState nodeCheckedState,
    bool isUserAction)
{
    if (!node || columns.empty())
        return NodeViewStatePatch();

    NodeViewStatePatch patch;
    ViewNodeData data;

    bool hasChangedData = false;
    for (const int column: columns)
    {

        if (!ViewNodeHelper(node).checkable(column)
            || ViewNodeHelper(node).checkedState(column, isUserAction) == nodeCheckedState)
        {
            continue;
        }

        ViewNodeDataBuilder(data).withCheckedState(column, nodeCheckedState, isUserAction);
        hasChangedData = true;
    }

    if (hasChangedData)
        patch.addUpdateDataStep(node->path(), data);
    return patch;

}

NodeViewStatePatch NodeViewStateReducer::setNodeChecked(
    const details::NodeViewState& state,
    const ViewNodePath& path,
    const details::ColumnSet& columns,
    Qt::CheckState nodeCheckedState,
    bool isUserAction)
{
    return setNodeChecked(state.nodeByPath(path), columns, nodeCheckedState, isUserAction);
}

NodeViewStatePatch NodeViewStateReducer::setNodeExpandedPatch(
    const details::NodeViewState& state,
    const ViewNodePath& path,
    bool value)
{
    const auto node = state.nodeByPath(path);
    if (!node || ViewNodeHelper(node).expanded() == value)
        return NodeViewStatePatch();

    NodeViewStatePatch patch;
    patch.addUpdateDataStep(path, ViewNodeDataBuilder().withExpanded(value));
    return patch;
}

details::NodeViewStatePatch NodeViewStateReducer::applyUserChangesPatch(
    const details::NodeViewState& state)
{
    static const auto kUserCheckStateRole = ViewNodeHelper::makeUserActionRole(Qt::CheckStateRole);

    NodeViewStatePatch result;

    ViewNodeHelper::ViewNodeHelper::forEachNode(state.rootNode,
        [&result](const NodePtr& node)
        {
            const auto& data = node->data();
            for (const int column: data.usedColumns())
            {
                if (!data.hasData(column, ViewNodeHelper::makeUserActionRole(Qt::CheckStateRole)))
                    continue;

                const auto& path = node->path();
                result.addRemoveDataStep(path, {{column, {kUserCheckStateRole}}});

                const auto& data = node->data();
                const auto userState = ViewNodeHelper(data).userCheckedState(column);
                const auto currentState = ViewNodeHelper(data).checkedState(column);
                if (userState == currentState)
                    continue;

                const auto updateCheckedData = ViewNodeDataBuilder()
                    .withCheckedState(column, userState).data();

                result.addUpdateDataStep(path, updateCheckedData);
            }
        });

    return result;
}

} // namespace node_view
} // namespace nx::vms::client::desktop
