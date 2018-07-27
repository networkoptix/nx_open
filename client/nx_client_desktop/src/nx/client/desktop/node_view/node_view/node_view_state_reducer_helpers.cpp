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
    const ColumnsSet& columns,
    Qt::CheckState state)
{
    for (const auto column: columns)
    {
        const auto data = ViewNodeDataBuilder().withCheckedState(column, state).data();
        patch.changedData.push_back({path, data});
    }
}

} // namespace helpers
} // namespace desktop
} // namespace client
} // namespace nx

