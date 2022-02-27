// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "../details/node_view_fwd.h"
#include "../details/node_view_state_patch.h"

namespace nx::vms::client::desktop {
namespace node_view {

class NodeViewStateReducer
{
public:
    static details::NodeViewStatePatch setNodeChecked(
        const details::NodePtr& node,
        const details::ColumnSet& columns,
        Qt::CheckState checkedState,
        bool isUserAction = false);

    static details::NodeViewStatePatch setNodeChecked(
        const details::NodeViewState& state,
        const details::ViewNodePath& path,
        const details::ColumnSet& columns,
        Qt::CheckState checkedState,
        bool isUserAction = false);

    static details::NodeViewStatePatch setNodeExpandedPatch(
        const details::NodeViewState& state,
        const details::ViewNodePath& path,
        bool expanded);

    static details::NodeViewStatePatch applyUserChangesPatch(
        const details::NodeViewState& state);

private:
    NodeViewStateReducer() = default;
    ~NodeViewStateReducer() = default;
};

} // namespace node_view
} // namespace nx::vms::client::desktop
