#pragma once

#include <nx/client/desktop/resource_views/node_view/nodes/view_node_fwd.h>

namespace nx {
namespace client {
namespace desktop {

struct NodeViewState
{
    NodePtr rootNode;

    bool checkable() const;
};

} // namespace desktop
} // namespace client
} // namespace nx
