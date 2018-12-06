#pragma once

#include "../details/node_view_state_patch.h"
#include "../details/node_view_fwd.h"

namespace nx::vms::client::desktop {
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
} // namespace nx::vms::client::desktop
