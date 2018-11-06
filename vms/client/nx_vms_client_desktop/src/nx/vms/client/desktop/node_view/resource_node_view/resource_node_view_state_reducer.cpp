#include "resource_node_view_state_reducer.h"

#include "resource_view_node_helpers.h"
#include "../details/node/view_node.h"
#include "../details/node_view_state_patch.h"

namespace nx::vms::client::desktop::node_view {

using namespace  details;

NodeViewStatePatch ResourceNodeViewStateReducer::setInvalidNodes(
    const NodeViewState& state,
    const PathList& paths,
    bool invalid)
{
    NodeViewStatePatch patch;
    for (const auto path: paths)
    {
        const auto node = state.rootNode->nodeAt(path);
        if (node && isValidNode(node) == invalid)
            patch.addChangeStep(path, getDataForInvalidNode(invalid));
    }
    return patch;
}

} // namespace nx::vms::client::desktop::node_view

