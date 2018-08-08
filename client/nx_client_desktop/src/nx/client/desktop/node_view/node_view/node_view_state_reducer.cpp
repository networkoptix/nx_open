#include "node_view_state_reducer.h"

#include "../details/node/view_node_data_builder.h"

namespace nx {
namespace client {
namespace desktop {
namespace node_view {

using namespace details;

NodeViewStatePatch NodeViewStateReducer::setNodeChecked(
    const ViewNodePath& path,
    const details::ColumnsSet& columns,
    Qt::CheckState state)
{
    NodeViewStatePatch patch;
    ViewNodeData data;
    for (const int column: columns)
        ViewNodeDataBuilder(data).withCheckedState(column, state);

    patch.addChangeStep(path, data);
    return patch;
}

NodeViewStatePatch NodeViewStateReducer::setNodeExpandedPatch(
    const ViewNodePath& path,
    bool expanded)
{
    NodeViewStatePatch patch;
    patch.addChangeStep(path, ViewNodeDataBuilder().withExpanded(expanded));
    return patch;
}

} // namespace node_view
} // namespace desktop
} // namespace client
} // namespace nx

