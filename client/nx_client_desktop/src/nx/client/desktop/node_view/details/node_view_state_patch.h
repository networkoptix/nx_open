#pragma once

#include <nx/client/desktop/resource_views/node_view/node_view_state.h>
#include <nx/client/desktop/resource_views/node_view/node/view_node.h>
#include <nx/client/desktop/resource_views/node_view/node/view_node_path.h>
#include <nx/client/desktop/resource_views/node_view/node/view_node_data.h>

#include <nx/utils/raii_guard.h>

namespace nx {
namespace client {
namespace desktop {

struct NodeViewStatePatch
{
    struct NodeDescription
    {
        ViewNodePath path;
        ViewNodeData data;
    };

    static NodeViewStatePatch fromRootNode(const NodePtr& node);

    using DataList = std::vector<NodeDescription>;
    DataList addedNodes;
    DataList changedData;
};

using GetNodeOperationGuard =
    std::function<QnRaiiGuardPtr (const NodeViewStatePatch::NodeDescription& description)>;

NodeViewState&& applyNodeViewPatch(
    NodeViewState&& state,
    const NodeViewStatePatch& patch,
    const GetNodeOperationGuard& getAddGuard = GetNodeOperationGuard(),
    const GetNodeOperationGuard& getDataChangedGuard = GetNodeOperationGuard());

} // namespace desktop
} // namespace client
} // namespace nx
