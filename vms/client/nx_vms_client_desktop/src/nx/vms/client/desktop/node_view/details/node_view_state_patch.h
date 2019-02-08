#pragma once

#include "node_view_state.h"
#include "node/view_node_path.h"
#include "node/view_node_data.h"

#include <nx/utils/scope_guard.h>

namespace nx::vms::client::desktop {
namespace node_view {
namespace details {

enum PatchStepOperation
{
    AppendNodeOperation,
    ChangeNodeOperation,
    RemoveNodeOperation
};

struct PatchStep
{
    PatchStepOperation operation;
    ViewNodePath path;
    ViewNodeData data;
};

struct NX_VMS_CLIENT_DESKTOP_API NodeViewStatePatch
{
    static NodeViewStatePatch fromRootNode(const NodePtr& node);
    static NodeViewStatePatch clearNodeView();

    using GetNodeOperationGuard = std::function<nx::utils::SharedGuardPtr (const PatchStep& step)>;
    NodeViewState&& applyTo(
        NodeViewState&& state,
        const GetNodeOperationGuard& getOperationGuard = {}) const;

    void addChangeStep(const ViewNodePath& path, const ViewNodeData& changedData);
    void addAppendStep(const ViewNodePath& path, const ViewNodeData& data);
    void addRemovalStep(const ViewNodePath& path);

    void appendPatchSteps(const NodeViewStatePatch& patch);

    using PatchStepList = std::vector<PatchStep>;
    PatchStepList steps;
};

} // namespace details
} // namespace node_view
} // namespace nx::vms::client::desktop
