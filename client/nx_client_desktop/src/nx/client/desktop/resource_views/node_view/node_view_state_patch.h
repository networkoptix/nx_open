#pragma once

#include <nx/client/desktop/resource_views/node_view/node_view_state.h>
#include <nx/client/desktop/resource_views/node_view/nodes/view_node.h>

namespace nx {
namespace client {
namespace desktop {

struct NodeViewStatePatch
{
    static NodeViewStatePatch fromRootNode(const NodePtr& node);

    NodeViewState apply(NodeViewState&& state) const;

    using NodeDescription = QPair<NodePath, ViewNode::Data>;
    using DataList = QList<NodeDescription>;
    DataList addedNodes;
    DataList changedData;
};

} // namespace desktop
} // namespace client
} // namespace nx
