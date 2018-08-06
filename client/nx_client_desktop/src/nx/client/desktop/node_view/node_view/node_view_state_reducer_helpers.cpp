#include "node_view_state_reducer_helpers.h"

#include "../details/node_view_state_patch.h"
#include "../details/node/view_node_data_builder.h"

namespace nx {
namespace client {
namespace desktop {
namespace node_view {

using namespace details;

void addCheckStateChangeToPatch(
    NodeViewStatePatch& patch,
    const ViewNodePath& path,
    const ColumnsSet& columns,
    Qt::CheckState state)
{
    for (const auto column: columns)
    {
        const auto data = ViewNodeDataBuilder().withCheckedState(column, state).data();
        patch.addChangeStep(path, data);
    }
}

} // namespace node_view
} // namespace desktop
} // namespace client
} // namespace nx

