#pragma once

#include "node/view_node_fwd.h"

namespace nx::vms::client::desktop {
namespace node_view {
namespace details {

struct NodeViewState
{
    NodeViewState() = default;
    NodeViewState(NodeViewState&& other) = default;
    NodeViewState& operator=(NodeViewState &&other) = default;

    NodePtr nodeByPath(const ViewNodePath& path) const;

    NodePtr rootNode;

private:
    NodeViewState(const NodeViewState& other) = delete;
    NodeViewState& operator=(const NodeViewState& other) = delete;
};

} // namespace details
} // namespace node_view
} // namespace nx::vms::client::desktop
