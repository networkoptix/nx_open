#pragma once

#include <nx/client/desktop/resource_views/node_view/nodes/view_node_fwd.h>

namespace nx {
namespace client {
namespace desktop {

struct NodeViewState
{
//    NodeViewState(const NodeViewState& other);
//    NodeViewState& operator=(const NodeViewState& other);

    NodePtr rootNode;

    bool checkable() const;
};

}// namespace desktop
} // namespace client
} // namespace nx
