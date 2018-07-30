#pragma once

#include "../details/node_view_state_patch.h"

namespace nx {
namespace client {
namespace desktop {
namespace node_view {

namespace details {

class ViewNodePath;
class NodeViewState;

} // namespace details

class SelectionNodeViewStateReducer
{
public:
    static details::NodeViewStatePatch setNodeSelected(
        const details::NodeViewState& state,
        const details::ColumnsSet& selectionColumns,
        const details::ViewNodePath& path,
        int column,
        Qt::CheckState checkedState);

private:
    SelectionNodeViewStateReducer() = default;
    ~SelectionNodeViewStateReducer() = default;
};

} // namespace node_view
} // namespace desktop
} // namespace client
} // namespace nx
