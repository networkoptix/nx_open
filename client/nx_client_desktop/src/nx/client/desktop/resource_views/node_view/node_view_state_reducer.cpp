#include "node_view_state_reducer.h"

#include <nx/client/desktop/resource_views/node_view/node_view_constants.h>

namespace nx {
namespace client {
namespace desktop {


NodeViewStatePatch NodeViewStateReducer::setNodeChecked(
    const ViewNodePath& path,
    Qt::CheckState checkedState)
{
    NodeViewStatePatch patch;
    auto node = NodeViewStatePatch::NodeDescription({path});
    node.data.data[node_view::checkMarkColumn][Qt::CheckStateRole] = checkedState;
    patch.changedData.append(node);
    return patch;
}

} // namespace desktop
} // namespace client
} // namespace nx

