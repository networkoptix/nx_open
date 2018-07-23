#pragma once

#include <nx/client/desktop/resource_views/node_view/node_view_state_patch.h>

namespace nx {
namespace client {
namespace desktop {

class NodeViewStateReducer
{
public:
    static NodeViewStatePatch setNodeChecked(
        const NodeViewState& state,
        const ViewNodePath& path,
        Qt::CheckState checkedState);

    static NodeViewStatePatch setNodeExpanded(
        const ViewNodePath& path,
        bool expanded);

private:
    NodeViewStateReducer() = default;
    ~NodeViewStateReducer() = default;
};

} // namespace desktop
} // namespace client
} // namespace nx
