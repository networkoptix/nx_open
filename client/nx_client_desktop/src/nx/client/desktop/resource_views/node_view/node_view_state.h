#pragma once

#include <nx/client/desktop/resource_views/node_view/node/view_node_fwd.h>

namespace nx {
namespace client {
namespace desktop {

struct NodeViewState
{
    NodeViewState() = default;
    NodeViewState(NodeViewState&& other) = default;
    NodeViewState& operator=(NodeViewState &&other) = default;

    NodePtr rootNode;

    bool checkable() const;
    NodePtr nodeByPath(const ViewNodePath& path) const;

private:
    NodeViewState(const NodeViewState& other) = default;
    NodeViewState& operator=(const NodeViewState& other) = default;

};

} // namespace desktop
} // namespace client
} // namespace nx
