#include "node_view_state_reducer.h"

namespace nx {
namespace client {
namespace desktop {


NodeViewStatePatch NodeViewStateReducer::setNodeChecked(
    const ViewNode::Path& path,
    Qt::CheckState checkedState)
{
    NodeViewStatePatch patch;
    patch.changedData[path][ViewNode::CheckMarkColumn][Qt::CheckStateRole] = checkedState;
    return patch;
}

} // namespace desktop
} // namespace client
} // namespace nx

