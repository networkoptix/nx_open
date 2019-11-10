#pragma once

#include <core/resource/resource_fwd.h>
#include <nx/vms/client/desktop/node_view/details/node_view_state_patch.h>
#include <nx/vms/client/desktop/node_view/resource_node_view/resource_node_view_constants.h>

namespace nx::vms::api { struct NetworkBlockData; }

namespace nx::vms::client::desktop {
namespace settings {

class PoESettingsReducer
{
public:
    static constexpr int kPortNumberProperty = node_view::lastResourceNodeViewProperty;

    static node_view::details::NodeViewStatePatch blockDataChanges(
        const node_view::details::NodeViewState& state,
        const nx::vms::api::NetworkBlockData& blockData,
        const QnResourcePool& resourcePool);
};

} // namespace settings
} // namespace nx::vms::client::desktop

