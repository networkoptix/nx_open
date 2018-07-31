#include "node_view_state_patch.h"

#include "node/view_node.h"

namespace {

using namespace nx::client::desktop::node_view::details;

void addNode(NodeViewStatePatch& patch, const NodePtr& node)
{
    if (!node)
        return;

    const auto path = node->path();
    patch.addNodeInsertionStep(path, node->nodeData());
    for (const auto child: node->children())
        addNode(patch, child);
}

void handleAddOperation(
    const PatchStep& step,
    NodeViewState& state,
    const NodeViewStatePatch::GetNodeOperationGuard& getOperationGuard)
{
    const auto node = ViewNode::create(step.data);
    if (step.path.isEmpty())
    {
        if (state.rootNode)
        {
            NX_EXPECT(false, "Can't add node that replaces root!");
        }
        else
        {
            const auto addNodeGuard = getOperationGuard(step);
            state.rootNode = node;
        }
    }
    else
    {
        const auto parentNode = state.nodeByPath(step.path.parentPath());
        const auto addNodeGuard = getOperationGuard(step);
        parentNode->addChild(node);
    }
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
}

void handleRemoveOperation(
    const PatchStep& step,
    NodeViewState& state,
    const NodeViewStatePatch::GetNodeOperationGuard& getOperationGuard)
{
    const auto node = state.rootNode->nodeAt(step.path);
    if (!node)
    {
        NX_EXPECT(false, "Can't delete node by this path");
        return;
    }

    for (const auto child: node->children())
    {
        const auto childRemoveStep = PatchStep{RemoveNodeOperation, child->path(), {}};
        handleRemoveOperation(childRemoveStep, state, getOperationGuard);
    }

    const auto dataChangedGuard = getOperationGuard(step);
    if (const auto parent = node->parent())
        parent->removeChild(node);
    else
        state.rootNode = NodePtr();
}

} // namespace

namespace nx {
namespace client {
namespace desktop {

NodeViewStatePatch NodeViewStatePatch::fromRootNode(const NodePtr& node)
{
    NodeViewStatePatch patch;
    addNode(patch, node);
    return patch;
}

NodeViewState&& NodeViewStatePatch::applyTo(
    NodeViewState&& state,
    const GetNodeOperationGuard& getOperationGuard) const
{
    static const auto emptyNodeGuard =
        [](const PatchStep& /*item*/) { return QnRaiiGuardPtr(); };

    const auto safeOperationGuard = getOperationGuard ? getOperationGuard : emptyNodeGuard;

    for (const auto step: steps)
    {
        switch(step.operation)
        {
            case AddNodeOperation:
                handleAddOperation(step, state, safeOperationGuard);
                break;
            case ChangeNodeOperation:
                handleChangeOperation(step, state, safeOperationGuard);
                break;
            case RemoveNodeOperation:
                handleRemoveOperation(step, state, safeOperationGuard);
                break;
            default:
                NX_EXPECT(false, "Operation is not supported.");

        }
    }
    return std::move(state);
}

void NodeViewStatePatch::addNodeChangeStep(
    const ViewNodePath& path,
    const ViewNodeData& changedData)
{
    steps.push_back({ChangeNodeOperation, path, changedData});
}

void NodeViewStatePatch::addNodeInsertionStep(
    const ViewNodePath& path,
        const ViewNodeData& data)
{
    steps.push_back({AddNodeOperation, path, data});
}

void NodeViewStatePatch::addNodeRemoveOperation(const ViewNodePath& path)
{
    steps.push_back({RemoveNodeOperation, path, {}});
}

} // namespace desktop
} // namespace client
} // namespace nx

