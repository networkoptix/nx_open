#include "node_view_state.h"

#include "node/view_node.h"

#include <nx/utils/log/log.h>

namespace nx {
namespace client {
namespace desktop {
namespace node_view {
namespace details {

NodePtr NodeViewState::nodeByPath(const ViewNodePath& path) const
{
    if (rootNode)
        return rootNode->nodeAt(path);

    NX_EXPECT(false, "Root is empty!");
    return NodePtr();
}

} // namespace details
} // namespace node_view
} // namespace desktop
} // namespace client
} // namespace nx

