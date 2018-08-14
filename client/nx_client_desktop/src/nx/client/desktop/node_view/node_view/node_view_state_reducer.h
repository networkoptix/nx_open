#pragma once

#include "../details/node_view_state_patch.h"

namespace nx {
namespace client {
namespace desktop {
namespace node_view {

namespace details { class NodeViewState; }

class NodeViewStateReducer
{
public:
    static details::NodeViewStatePatch setNodeChecked(
        const details::NodePtr& node,
        const details::ColumnSet& columns,
        Qt::CheckState checkedState);

    static details::NodeViewStatePatch setNodeChecked(
        const details::NodeViewState& state,
        const details::ViewNodePath& path,
        const details::ColumnSet& columns,
        Qt::CheckState checkedState);

    static details::NodeViewStatePatch setNodeExpandedPatch(
        const details::NodeViewState& state,
        const details::ViewNodePath& path,
        bool expanded);

private:
    NodeViewStateReducer() = default;
    ~NodeViewStateReducer() = default;
};

} // namespace node_view
} // namespace desktop
} // namespace client
} // namespace nx
