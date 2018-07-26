#include "node_view_state.h"

#include <nx/client/desktop/resource_views/node_view/node/view_node.h>
#include <nx/client/desktop/resource_views/node_view/node/view_node_helpers.h>

namespace {

using namespace nx::client::desktop;

//bool checkableInternal(const NodePtr& root)
//{
//    if (helpers::isCheckable(root))
//        return true;

//    if (!root->childrenCount())
//        return false;

//    const auto children = root->children();
//    return std::any_of(children.begin(), children.end(),
//        [](const NodePtr& child) { return checkableInternal(child); });
//}

} // namespace

namespace nx {
namespace client {
namespace desktop {

bool NodeViewState::checkable() const
{
    return false;//return checkableInternal(rootNode);
}

NodePtr NodeViewState::nodeByPath(const ViewNodePath& path) const
{
    if (rootNode)
        return rootNode->nodeAt(path);

    NX_EXPECT(false, "Root is empty!");
    return NodePtr();
}

} // namespace desktop
} // namespace client
} // namespace nx

