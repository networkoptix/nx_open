#pragma once

#include <nx/client/desktop/resource_views/node_view/nodes/view_node.h>
#include <nx/client/desktop/resource_views/node_view/node_view_state_patch.h>

namespace nx {
namespace client {
namespace desktop {

class NodeViewStateReducer
{
public:
    static NodeViewStatePatch setNodeChecked(
        const NodePath& path,
        Qt::CheckState checkedState);

private:
    NodeViewStateReducer() = default;
    ~NodeViewStateReducer() = default;
};

} // namespace desktop
} // namespace client
} // namespace nx
