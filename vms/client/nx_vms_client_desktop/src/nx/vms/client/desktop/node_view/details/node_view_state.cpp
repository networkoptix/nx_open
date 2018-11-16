#include "node_view_state.h"

#include "node/view_node.h"

#include <nx/utils/log/log.h>

namespace nx::vms::client::desktop {
namespace node_view {
namespace details {

NodePtr NodeViewState::nodeByPath(const ViewNodePath& path) const
{
    if (rootNode)
        return rootNode->nodeAt(path);

    NX_ASSERT(false, "Root is empty!");
    return NodePtr();
}

} // namespace details
} // namespace node_view
} // namespace nx::vms::client::desktop

