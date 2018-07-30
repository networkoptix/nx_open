#pragma once

#include "../details/node_view_state_patch.h"

#include <core/resource/resource_fwd.h>

namespace nx {
namespace client {
namespace desktop {
namespace node_view {

class ResourceNodeViewStateReducer
{
public:
    static details::NodeViewStatePatch getLeafResourcesCheckedPatch(
        const details::ColumnsSet& columns,
        const details::NodeViewState& state,
        const QnResourceList& resources);
};

} // namespace node_view
} // namespace desktop
} // namespace client
} // namespace nx
