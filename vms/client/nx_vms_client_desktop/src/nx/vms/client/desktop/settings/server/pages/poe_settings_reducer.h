#pragma once

#include <core/resource/resource_fwd.h>
#include <nx/vms/client/core/poe_settings/poe_controller.h>
#include <nx/vms/client/desktop/node_view/details/node_view_state_patch.h>
#include <nx/vms/client/desktop/node_view/resource_node_view/resource_node_view_constants.h>
#include <nx/vms/client/desktop/settings/server/pages/poe_settings_state.h>

namespace nx::vms::client::desktop {
namespace settings {

class PoESettingsReducer
{
public:
    static constexpr int kPortNumberProperty = node_view::lastResourceNodeViewProperty;

    static node_view::details::NodeViewStatePatch blockDataChangesPatch(
        const node_view::details::NodeViewState& state,
        const core::PoEController::OptionalBlockData& blockData,
        QnResourcePool* resourcePool);

    static PoESettingsStatePatch::BoolOptional poeOverBudgetChanges(
        const PoESettingsState& state,
        const core::PoEController::OptionalBlockData& blockData);

    static PoESettingsStatePatch::BoolOptional blockUiChanges(
        const PoESettingsState& state,
        const bool blockUi);
};

} // namespace settings
} // namespace nx::vms::client::desktop

