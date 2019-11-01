#pragma once

#include <core/resource/resource_fwd.h>
#include <nx/vms/client/desktop/node_view/details/node_view_state_patch.h>

namespace nx::vms::api { struct NetworkBlockData; }

namespace nx::vms::client::desktop {
namespace settings {

class PoESettingsTableStateReducer
{
public:
    static node_view::details::NodeViewStatePatch applyBlockDataChanges(
        const node_view::details::NodeViewState& state,
        const nx::vms::api::NetworkBlockData& blockData,
        const QnResourcePool& resourcePool);

};

} // namespace settings
} // namespace nx::vms::client::desktop
