#include "node_view_state_reducer.h"

#include <nx/client/desktop/resource_views/node_view/node_view_constants.h>

namespace nx {
namespace client {
namespace desktop {


NodeViewStatePatch NodeViewStateReducer::setNodeChecked(
    const ViewNode::Path& path,
    Qt::CheckState checkedState)
{
    NodeViewStatePatch patch;
    patch.changedData[path][node_view::checkMarkColumn][Qt::CheckStateRole] = checkedState;
    return patch;
}

} // namespace desktop
} // namespace client
} // namespace nx

