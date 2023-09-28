// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <core/resource/resource_fwd.h>
#include <nx/vms/client/desktop/node_view/details/node_view_state_patch.h>
#include <nx/vms/client/desktop/node_view/resource_node_view/resource_node_view_constants.h>
#include <nx/vms/client/desktop/resource_properties/server/poe/poe_settings_state.h>
#include <nx/vms/client/desktop/resource_properties/server/poe/poe_settings_widget.h>

namespace nx::vms::api { struct NetworkBlockData; }

namespace nx::vms::client::desktop {

class PoeSettingsReducer
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

    static PoeSettingsStatePatch::BoolOptional poeOverBudgetChanges(
        const PoeSettingsState& state,
        const nx::vms::api::NetworkBlockData& data);

    static PoeSettingsStatePatch::BoolOptional blockUiChanges(
        const PoeSettingsState& state,
        const bool blockUi);

    static PoeSettingsStatePatch::BoolOptional showPreloaderChanges(
        const PoeSettingsState& state,
        const bool value);

    static PoeSettingsStatePatch::BoolOptional autoUpdatesChanges(
        const PoeSettingsState& state,
        const bool value);
};

} // namespace nx::vms::client::desktop
