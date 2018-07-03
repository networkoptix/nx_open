#pragma once

#include <nx/client/desktop/resource_views/node_view/node_view_state.h>
#include <nx/client/desktop/resource_views/node_view/nodes/view_node.h>

namespace nx {
namespace client {
namespace desktop {

struct NodeViewStatePatch
{
//    using PathList = QList<ViewNode::Path>;
//    PathList removedNodes;

    using DataChange = QHash<ViewNode::Path, ViewNode::Data::ColumnDataHash>;

    DataChange changedData;
    NodeViewState apply(NodeViewState&& state) const;
};

} // namespace desktop
} // namespace client
} // namespace nx
