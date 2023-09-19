// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "node_view_state_patch.h"

#include "node/view_node.h"

namespace {

using namespace nx::vms::client::desktop::node_view::details;

void addNode(NodeViewStatePatch& patch, const NodePtr& node)
{
    if (!node)
        return;

    if (node->parent())
    {
        const auto parentPath = node->path().parentPath();
        patch.addAppendStep(parentPath, node->data());
    }

    for (const auto child: node->children())
        addNode(patch, child);
}

void handleAddOperation(
    const PatchStep& step,
    NodeViewState& state,
    const NodeViewStatePatch::GetNodeOperationGuard& getOperationGuard)
{
    if (state.rootNode.isNull())
        state.rootNode = ViewNode::create();

    const auto node = ViewNode::create(step.operationData.data.value<ViewNodeData>());
    const auto parentNode = state.nodeByPath(step.path);
    const auto addNodeGuard = getOperationGuard(step);
    parentNode->addChild(node);
}

void handleChangeOperation(
    const PatchStep& step,
    NodeViewState& state,
    const NodeViewStatePatch::GetNodeOperationGuard& getOperationGuard)
{
    if (const auto node = state.rootNode->nodeAt(step.path))
    {
        const auto dataChangedGuard = getOperationGuard(step);
        node->applyNodeData(step.operationData.data.value<ViewNodeData>());
    }
    else
    {
        NX_ASSERT(false, "Can't change node by this path!");
    }
}

void handleRemoveOperation(
    const PatchStep& step,
    NodeViewState& state,
    const NodeViewStatePatch::GetNodeOperationGuard& getOperationGuard)
{
    const auto node = state.rootNode ? state.rootNode->nodeAt(step.path) : NodePtr();
    if (!node)
    {
        NX_ASSERT(false, "Can't delete node by this path!");
        return;
    }

    for (const auto child: node->children())
    {
        const auto childRemoveStep = PatchStep{child->path(), {removeNodeOperation}};
        handleRemoveOperation(childRemoveStep, state, getOperationGuard);
    }

    if (const auto parent = node->parent())
    {
        const auto dataChangedGuard = getOperationGuard(step);
        parent->removeChild(node);
    }
    else
    {
        state.rootNode = NodePtr();
    }
}

void handleRemoveDataOperation(
    const PatchStep& step,
    NodeViewState& state,
    const NodeViewStatePatch::GetNodeOperationGuard& getOperationGuard)
{
    const auto node = state.rootNode ? state.rootNode->nodeAt(step.path) : NodePtr();
    if (!node)
    {
        NX_ASSERT(false, "Can't delete node data by this path!");
        return;
    }

    const auto removeDataGuard = getOperationGuard(step);
    node->removeNodeData(step.operationData.data.value<RemoveData>());
}

} // namespace

namespace nx::vms::client::desktop {

NodeViewStatePatch NodeViewStatePatch::fromRootNode(const NodePtr& node)
{
    NodeViewStatePatch patch;
    addNode(patch, node);
    return patch;
}

NodeViewStatePatch NodeViewStatePatch::clearNodeViewPatch()
{
    NodeViewStatePatch patch;
    patch.addRemovalStep(ViewNodePath());
    return patch;
}

bool NodeViewStatePatch::isEmpty() const
{
    return steps.empty();
}

NodeViewState&& NodeViewStatePatch::applyTo(
    NodeViewState&& state,
    const GetNodeOperationGuard& getOperationGuard) const
{
    static const auto emptyNodeGuard =
        [](const PatchStep& /*item*/) { return utils::SharedGuardPtr(); };

    const auto safeOperationGuard = getOperationGuard ? getOperationGuard : emptyNodeGuard;

    for (const auto step: steps)
    {
        switch(step.operationData.operation)
        {
            case appendNodeOperation:
                handleAddOperation(step, state, safeOperationGuard);
                break;
            case updateDataOperation:
                handleChangeOperation(step, state, safeOperationGuard);
                break;
            case removeNodeOperation:
                handleRemoveOperation(step, state, safeOperationGuard);
                break;
            case removeDataOperation:
                handleRemoveDataOperation(step, state, safeOperationGuard);
                break;
            default:
                NX_ASSERT(false, "Operation is not supported.");

        }
    }
    return std::move(state);
}

void NodeViewStatePatch::addRemoveDataStep(
    const ViewNodePath& path,
    const ColumnRoleHash& roleHash)
{
    steps.push_back({path, {removeDataOperation, QVariant::fromValue(roleHash)}});
}

void NodeViewStatePatch::addUpdateDataStep(
    const ViewNodePath& path,
    const ViewNodeData& updateData)
{
    steps.push_back({path, {updateDataOperation, QVariant::fromValue(updateData)}});
}

void NodeViewStatePatch::addAppendStep(
    const ViewNodePath& parentPath,
    const ViewNodeData& data)
{
    steps.push_back({parentPath, {appendNodeOperation, QVariant::fromValue(data)}});
}

void NodeViewStatePatch::addRemovalStep(const ViewNodePath& path)
{
    steps.push_back({path, {removeNodeOperation}});
}

void NodeViewStatePatch::appendPatchStep(const PatchStep& step)
{
    steps.push_back(step);
}

void NodeViewStatePatch::appendPatchSteps(const NodeViewStatePatch& patch)
{
    steps.insert(steps.end(), patch.steps.begin(), patch.steps.end());
}

} // namespace nx::vms::client::desktop
