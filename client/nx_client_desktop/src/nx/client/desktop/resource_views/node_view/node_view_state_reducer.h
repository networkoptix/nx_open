#pragma once

#include <nx/client/desktop/resource_views/node_view/nodes/view_node.h>
#include <nx/client/desktop/resource_views/node_view/node_view_state.h>

namespace nx {
namespace client {
namespace desktop {

class NodeViewStateReducer
{
public:
    static NodeViewState setNodeChecked(
        const NodeViewState& state,
        const ViewNode::Path& path,
        Qt::CheckState checkedState);

private:
    NodeViewStateReducer() = default;
    ~NodeViewStateReducer() = default;
};

} // namespace desktop
} // namespace client
} // namespace nx
