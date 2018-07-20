#pragma once

#include <core/resource/resource_fwd.h>
#include <nx/client/desktop/resource_views/node_view/node_view_state.h>
#include <nx/client/desktop/resource_views/node_view/node_view_state_patch.h>

namespace nx {
namespace client {
namespace desktop {

class ResourceNodeViewStateReducer
{
public:
    static NodeViewStatePatch getLeafResourcesCheckedPatch(
        const NodeViewState& state,
        const QnResourceList& resources);
};

} // namespace desktop
} // namespace client
} // namespace nx
