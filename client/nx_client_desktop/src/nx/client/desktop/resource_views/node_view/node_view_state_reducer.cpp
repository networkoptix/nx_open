#include "node_view_state_reducer.h"

namespace nx {
namespace client {
namespace desktop {


NodeViewState NodeViewStateReducer::setNodeChecked(
    const NodeViewState& state,
    const ViewNode::Path& path,
    Qt::CheckState checkedState)
{
    NodeViewState copy(state);
    const auto node = copy.rootNode->nodeAt(path);
    node->setCheckedState(checkedState);
    return copy;
}

} // namespace desktop
} // namespace client
} // namespace nx

