#include "node_view_state.h"

#include <nx/client/desktop/resource_views/node_view/nodes/view_node.h>

namespace {

using namespace nx::client::desktop;

bool checkableInternal(const NodePtr& root)
{
    if (root->checkable())
        return true;

    if (!root->childrenCount())
        return false;

    const auto children = root->children();
    return std::any_of(children.begin(), children.end(),
        [](const NodePtr& child) { return checkableInternal(child); });
}

} // namespace

namespace nx {
namespace client {
namespace desktop {

bool NodeViewState::checkable() const
{
    return checkableInternal(rootNode);
}

NodePtr NodeViewState::nodeByPath(const NodePath& path) const
{
    return rootNode ? rootNode->nodeAt(path) : NodePtr();
}

} // namespace desktop
} // namespace client
} // namespace nx

