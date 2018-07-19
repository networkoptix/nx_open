#include "node_view_state_patch.h"

#include <nx/client/desktop/resource_views/node_view/nodes/view_node_path.h>

namespace {

using namespace nx::client::desktop;
void addNode(NodeViewStatePatch::DataList& data, const NodePtr& node)
{
    if (!node)
        return;

    const auto path = node->path();
    data.append({path, node->nodeData()});
    for (const auto child: node->children())
        addNode(data, child);
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

NodeViewState&& applyNodeViewPatch(
    NodeViewState&& state,
    const NodeViewStatePatch& patch,
    const GetNodeOperationGuard& getNodeGuard,
    const GetNodeOperationGuard& getDataChangedGuard)
{
    static const auto emptyNodeGuard =
        [](const NodeViewStatePatch::NodeDescription& /*description*/){ return QnRaiiGuardPtr(); };

    const auto safeGetAddNodeGuard = getNodeGuard ? getNodeGuard : emptyNodeGuard;
    const auto safeGetDataChangedGuard = getDataChangedGuard ? getDataChangedGuard : emptyNodeGuard;

    for (const auto description: patch.addedNodes)
    {
        const auto node = ViewNode::create(description.data);
        if (description.path.isEmpty())
        {
            if (state.rootNode)
            {
                // TODO: add ut for this case
                NX_EXPECT(false, "Can't add node replacing root one!");
                continue;
            }

            const auto addNodeGuard = safeGetAddNodeGuard(description);
            state.rootNode = node;
            continue;
        }
        const auto parentNode = state.nodeByPath(description.path.parentPath());
        const auto addNodeGuard = safeGetAddNodeGuard(description);
        parentNode->addChild(node);
    }

    for (const auto description: patch.changedData)
    {
        if (const auto node = state.rootNode->nodeAt(description.path))
        {
            const auto dataChangedGuard = safeGetDataChangedGuard(description);
            node->applyNodeData(description.data);
        }
    }
    return std::move(state);
}

} // namespace desktop
} // namespace client
} // namespace nx

