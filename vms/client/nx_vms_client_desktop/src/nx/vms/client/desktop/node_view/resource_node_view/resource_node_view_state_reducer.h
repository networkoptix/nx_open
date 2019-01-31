#pragma once

#include "../details/node_view_fwd.h"

namespace nx::vms::client::desktop::node_view {

class ResourceNodeViewStateReducer
{
public:
    static details::NodeViewStatePatch setInvalidNodes(
        const details::NodeViewState& state,
        const details::PathList& paths,
        bool invalid);
};

} // namespace nx::vms::client::desktop::node_view
