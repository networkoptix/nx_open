#include "node_view_state_patch.h"

namespace nx {
namespace client {
namespace desktop {

NodeViewState NodeViewStatePatch::apply(NodeViewState&& state) const
{

    for (auto it = changedData.begin(); it != changedData.end(); ++it)
    {
        const auto& path = it.key();
        if (const auto node = state.rootNode->nodeAt(path))
            node->applyData(it.value());
    }
    return state;
}

} // namespace desktop
} // namespace client
} // namespace nx

