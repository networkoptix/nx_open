#include "node_view_store.h"

#include <nx/client/desktop/resource_views/node_view/node_view_state_reducer.h>

namespace nx {
namespace client {
namespace desktop {

void NodeViewStore::setNodeChecked(const ViewNode::Path& path, Qt::CheckState checkedState)
{
    execute(
        [path, checkedState](const NodeViewState& state)
        {
            return NodeViewStateReducer::setNodeChecked(state, path, checkedState);
        });
}

} // namespace desktop
} // namespace client
} // namespace nx

