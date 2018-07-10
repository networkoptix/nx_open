#include "node_view_state_patch.h"

#include <nx/client/desktop/resource_views/node_view/nodes/view_node_path.h>

namespace {

using namespace nx::client::desktop;
void addNode(NodeViewStatePatch::DataList& data, const NodePtr& node)
{
    if (!node)
        return;

    const auto path = node->path();
    data.append(NodeViewStatePatch::NodeDescription(path, node->nodeData()));
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

NodeViewState NodeViewStatePatch::apply(NodeViewState&& state) const
{
    for (const auto dataDescription: addedNodes)
    {
        const auto path = dataDescription.first;
        const auto nodeData = dataDescription.second;
        const auto node = ViewNode::create(nodeData);
        if (path->isEmpty())
        {
            if (state.rootNode)
            {
                // TODO: add ut for this case
                NX_EXPECT(false, "Can't add node replacing root one!");
                continue;
            }
            state.rootNode = node;
            continue;
        }
        const auto parentNode = state.nodeByPath(path->parent());
        parentNode->addChild(node);
    }

    for (const auto dataDescription: changedData)
    {
        const auto& path = dataDescription.first;
        const auto& nodeData = dataDescription.second;
        if (const auto node = state.rootNode->nodeAt(path))
            node->applyNodeData(nodeData);
    }
    return state;
}

} // namespace desktop
} // namespace client
} // namespace nx

