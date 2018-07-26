#pragma once

#include <nx/client/desktop/resource_views/node_view/node_view_state_patch.h>

namespace nx {
namespace client {
namespace desktop {
namespace details {

class NodeViewStateReducer
{
public:
    static NodeViewStatePatch setNodeChecked(
        const ViewNodePath& path,
        int column,
        Qt::CheckState state);

    static NodeViewStatePatch setNodeExpanded(
        const ViewNodePath& path,
        bool expanded);

private:
    NodeViewStateReducer() = default;
    ~NodeViewStateReducer() = default;
};

} // namespace details
} // namespace desktop
} // namespace client
} // namespace nx
