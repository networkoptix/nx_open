#pragma once

#include "../details/node_view_state_patch.h"

namespace nx {
namespace client {
namespace desktop {
namespace node_view {

class NodeViewStateReducer
{
public:
    static details::NodeViewStatePatch setNodeChecked(
        const details::ViewNodePath& path,
        int column,
        Qt::CheckState state);

    static details::NodeViewStatePatch setNodeExpandedPatch(
        const details::ViewNodePath& path,
        bool expanded);

private:
    NodeViewStateReducer() = default;
    ~NodeViewStateReducer() = default;
};

} // namespace node_view
} // namespace desktop
} // namespace client
} // namespace nx
