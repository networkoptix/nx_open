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
    auto data = node->nodeData();
    data.checkedState = checkedState;
    node->setNodeData(data);
    return copy;
}

} // namespace desktop
} // namespace client
} // namespace nx

