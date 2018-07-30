#include "node_view_state_patch.h"

#include "node/view_node.h"

namespace {

using namespace nx::client::desktop::node_view::details;

void addNode(NodeViewStatePatch::ItemList& items, const NodePtr& node)
{
    if (!node)
        return;

    const auto path = node->path();
    items.push_back({path, node->nodeData()});
    for (const auto child: node->children())
        addNode(items, child);
}

} // namespace

namespace nx {
namespace client {
namespace desktop {

NodeViewStatePatch NodeViewStatePatch::fromRootNode(const NodePtr& node)
{
    NodeViewStatePatch patch;
    addNode(patch.addedNodes, node);
    return patch;
}

NodeViewState&& NodeViewStatePatch::applyTo(
    NodeViewState&& state,
    const GetNodeOperationGuard& getNodeGuard,
    const GetNodeOperationGuard& getDataChangedGuard) const
{
    static const auto emptyNodeGuard =
        [](const PatchItem& /*item*/) { return QnRaiiGuardPtr(); };

    const auto safeGetAddNodeGuard = getNodeGuard ? getNodeGuard : emptyNodeGuard;
    const auto safeGetDataChangedGuard = getDataChangedGuard ? getDataChangedGuard : emptyNodeGuard;

    for (const auto item: addedNodes)
    {
        const auto node = ViewNode::create(item.data);
        if (item.path.isEmpty())
        {
            if (state.rootNode)
            {
                NX_EXPECT(false, "Can't add node that replaces root!");
                continue;
            }

            const auto addNodeGuard = safeGetAddNodeGuard(item);
            state.rootNode = node;
        }
        else
        {
            const auto parentNode = state.nodeByPath(item.path.parentPath());
            const auto addNodeGuard = safeGetAddNodeGuard(item);
            parentNode->addChild(node);
        }
    }

    for (const auto item: changedData)
    {
        if (const auto node = state.rootNode->nodeAt(item.path))
        {
            const auto dataChangedGuard = safeGetDataChangedGuard(item);
            node->applyNodeData(item.data);
        }
    }
    return std::move(state);
}

} // namespace desktop
} // namespace client
} // namespace nx

