#pragma once

#include <nx/client/desktop/resource_views/node_view/node_view_state.h>
#include <nx/client/desktop/resource_views/node_view/nodes/view_node.h>
#include <nx/client/desktop/resource_views/node_view/nodes/view_node_path.h>

namespace nx {
namespace client {
namespace desktop {

struct NodeViewStatePatch
{
    static NodeViewStatePatch fromRootNode(const NodePtr& node);

    NodeViewState apply(NodeViewState&& state) const;

    struct NodeDescription
    {
        ViewNodePath path;
        ViewNode::Data data;
    };

    using DataList = QList<NodeDescription>;
    DataList addedNodes;
    DataList changedData;
};

} // namespace desktop
} // namespace client
} // namespace nx
