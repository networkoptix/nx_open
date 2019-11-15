#pragma once

#include <core/resource/resource_fwd.h>
#include <nx/vms/client/desktop/node_view/details/node_view_state_patch.h>
#include <nx/vms/client/desktop/node_view/resource_node_view/resource_node_view_constants.h>
#include <nx/vms/client/desktop/resource_properties/server/poe/poe_settings_widget.h>
#include <nx/vms/client/desktop/resource_properties/server/poe/poe_settings_state.h>

namespace nx::vms::api { struct NetworkBlockData; }

namespace nx::vms::client::desktop {
namespace settings {

class PoESettingsReducer
{
public:
    static constexpr int kPortNumberProperty = node_view::lastResourceNodeViewProperty;

    static node_view::details::NodeViewStatePatch blockDataChangesPatch(
        const node_view::details::NodeViewState& state,
        const nx::vms::api::NetworkBlockData& data,
        QnResourcePool* resourcePool);

    static node_view::details::NodeViewStatePatch totalsDataChangesPatch(
        const node_view::details::NodeViewState& state,
        const nx::vms::api::NetworkBlockData& data);

    static PoESettingsStatePatch::BoolOptional poeOverBudgetChanges(
        const PoESettingsState& state,
        const nx::vms::api::NetworkBlockData& data);

    static PoESettingsStatePatch::BoolOptional blockUiChanges(
        const PoESettingsState& state,
        const bool blockUi);

    static PoESettingsStatePatch::BoolOptional showPreloaderChanges(
        const PoESettingsState& state,
        const bool value);

    static PoESettingsStatePatch::BoolOptional autoUpdatesChanges(
        const PoESettingsState& state,
        const bool value);
};

} // namespace settings
} // namespace nx::vms::client::desktop

