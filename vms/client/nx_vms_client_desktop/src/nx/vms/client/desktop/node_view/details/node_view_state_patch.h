#pragma once

#include "node_view_state.h"
#include "node/view_node_path.h"

#include <nx/utils/scope_guard.h>

namespace nx::vms::client::desktop {
namespace node_view {
namespace details {

class ViewNodeData;

enum PatchStepOperation
{
    appendNodeOperation,
    overrideDataOperation,
    removeDataOperation,
    removeNodeOperation
};

struct OperationData
{
    PatchStepOperation operation;
    QVariant data;
};

struct PatchStep
{
    ViewNodePath path;
    OperationData operationData;
};

using PatchStepList = std::vector<PatchStep>;

struct NX_VMS_CLIENT_DESKTOP_API NodeViewStatePatch
{
    static NodeViewStatePatch fromRootNode(const NodePtr& node);
    static NodeViewStatePatch clearNodeView();

    using GetNodeOperationGuard = std::function<nx::utils::SharedGuardPtr (const PatchStep& step)>;
    NodeViewState&& applyTo(
        NodeViewState&& state,
        const GetNodeOperationGuard& getOperationGuard = {}) const;

    void addRemoveDataStep(const ViewNodePath& path, const ColumnRoleHash& roleHash);
    void addOverrideDataStep(const ViewNodePath& path, const ViewNodeData& overrideData);
    void addAppendStep(const ViewNodePath& path, const ViewNodeData& data);
    void addRemovalStep(const ViewNodePath& path);

    void appendPatchStep(const PatchStep& step);
    void appendPatchSteps(const NodeViewStatePatch& patch);

    PatchStepList steps;
};

} // namespace details
} // namespace node_view
} // namespace nx::vms::client::desktop
