#pragma once

#include "node_view_state.h"
#include "node/view_node_path.h"
#include "node/view_node_data.h"

#include <nx/utils/raii_guard.h>

namespace nx {
namespace client {
namespace desktop {
namespace node_view {
namespace details {

struct PatchItem
{
    ViewNodePath path;
    ViewNodeData data;
};

struct NodeViewStatePatch
{
    static NodeViewStatePatch fromRootNode(const NodePtr& node);

    using GetNodeOperationGuard = std::function<QnRaiiGuardPtr (const PatchItem& item)>;
    NodeViewState&& applyTo(
        NodeViewState&& state,
        const GetNodeOperationGuard& getAddGuard = GetNodeOperationGuard(),
        const GetNodeOperationGuard& getDataChangedGuard = GetNodeOperationGuard()) const;

    using ItemList = std::vector<PatchItem>;
    ItemList addedNodes;
    ItemList changedData;
};

} // namespace details
} // namespace node_view
} // namespace desktop
} // namespace client
} // namespace nx
