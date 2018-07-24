#include "node_view_state_reducer_helpers.h"

#include <nx/client/desktop/resource_views/node_view/node_view_state_patch.h>
#include <nx/client/desktop/resource_views/node_view/node/view_node_data_builder.h>

namespace nx {
namespace client {
namespace desktop {
namespace details {

void addCheckStateChangeToPatch(
    NodeViewStatePatch& patch,
    const ViewNodePath& path,
    Qt::CheckState state)
{
    auto nodeDescirption = NodeViewStatePatch::NodeDescription({path});
    ViewNodeDataBuilder(nodeDescirption.data).withCheckedState(state);
    patch.changedData.push_back(nodeDescirption);
}

} // namespace helpers
} // namespace desktop
} // namespace client
} // namespace nx

