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

enum PatchStepOperation
{
    AddNodeOperation,
    ChangeNodeOperation,
    RemoveNodeOperation
};

struct PatchStep
{
    PatchStepOperation operation;
    ViewNodePath path;
    ViewNodeData data;
};

struct NodeViewStatePatch
{
    static NodeViewStatePatch fromRootNode(const NodePtr& node);

    using GetNodeOperationGuard = std::function<QnRaiiGuardPtr (const PatchStep& step)>;
    NodeViewState&& applyTo(
        NodeViewState&& state,
        const GetNodeOperationGuard& getOperationGuard = {}) const;

    void addNodeChangeStep(const ViewNodePath& path, const ViewNodeData& changedData);
    void addNodeInsertionStep(const ViewNodePath& path, const ViewNodeData& data);
    void addNodeRemoveOperation(const ViewNodePath& path);

    using PatchStepList = std::vector<PatchStep>;
    PatchStepList steps;
};

} // namespace details
} // namespace node_view
} // namespace desktop
} // namespace client
} // namespace nx
