#pragma once

#include <nx/client/desktop/resource_views/node_view/node_view_state_patch.h>

namespace nx {
namespace client {
namespace desktop {

class ViewNodePath;
class NodeViewState;

namespace node_view {

class SelectionNodeViewStateReducer
{
public:
    static NodeViewStatePatch setNodeSelected(
        const NodeViewState& state,
        const ColumnsSet& selectionColumns,
        const ViewNodePath& path,
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
