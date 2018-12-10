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
        patch.addAppendStep(parentPath, node->nodeData());
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

    const auto node = ViewNode::create(step.data);
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
        node->applyNodeData(step.data);
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
        const auto childRemoveStep = PatchStep{RemoveNodeOperation, child->path(), {}};
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

} // namespace

namespace nx::vms::client::desktop {

NodeViewStatePatch NodeViewStatePatch::fromRootNode(const NodePtr& node)
{
    NodeViewStatePatch patch;
    addNode(patch, node);
    return patch;
}

NodeViewStatePatch NodeViewStatePatch::clearNodeView()
{
    NodeViewStatePatch patch;
    patch.addRemovalStep(ViewNodePath());
    return patch;
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
        switch(step.operation)
        {
            case AppendNodeOperation:
                handleAddOperation(step, state, safeOperationGuard);
                break;
            case ChangeNodeOperation:
                handleChangeOperation(step, state, safeOperationGuard);
                break;
            case RemoveNodeOperation:
                handleRemoveOperation(step, state, safeOperationGuard);
                break;
            default:
                NX_ASSERT(false, "Operation is not supported.");

        }
    }
    return std::move(state);
}

void NodeViewStatePatch::addChangeStep(
    const ViewNodePath& path,
    const ViewNodeData& changedData)
{
    steps.push_back({ChangeNodeOperation, path, changedData});
}

void NodeViewStatePatch::addAppendStep(
    const ViewNodePath& parentPath,
    const ViewNodeData& data)
{
    steps.push_back({AppendNodeOperation, parentPath, data});
}

void NodeViewStatePatch::addRemovalStep(const ViewNodePath& path)
{
    steps.push_back({RemoveNodeOperation, path, {}});
}

void NodeViewStatePatch::appendPatchSteps(const NodeViewStatePatch& patch)
{
    steps.insert(steps.end(), patch.steps.begin(), patch.steps.end());
}

} // namespace nx::vms::client::desktop

