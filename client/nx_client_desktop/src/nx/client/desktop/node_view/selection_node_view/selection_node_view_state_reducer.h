#pragma once

#include "../details/node_view_state_patch.h"
#include "../details/node_view_fwd.h"

namespace nx {
namespace client {
namespace desktop {
namespace node_view {

class SelectionNodeViewStateReducer
{
public:
    static details::NodeViewStatePatch setNodeSelected(
        const details::NodeViewState& state,
        const details::ColumnSet& selectionColumns,
        const details::ViewNodePath& path,
        Qt::CheckState checkedState);

private:
    SelectionNodeViewStateReducer() = default;
    ~SelectionNodeViewStateReducer() = default;
};

} // namespace node_view
} // namespace desktop
} // namespace client
} // namespace nx
