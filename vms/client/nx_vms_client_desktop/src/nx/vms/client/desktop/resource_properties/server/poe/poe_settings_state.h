#pragma once

#include <nx/vms/client/desktop/node_view/details/node_view_state_patch.h>

#include <nx/utils/std/optional.h>

namespace nx::vms::client::desktop {

struct PoeSettingsState
{
    bool autoUpdates = true;
    bool showPreloader = true;
    bool showPoeOverBudgetWarning = false;
    bool blockUi = false;
    bool hasChanges = false;
};

struct PoeSettingsStatePatch
{
    using BoolOptional = std::optional<bool>;
    BoolOptional autoUpdates;
    BoolOptional showPoeOverBudgetWarning;
    BoolOptional showPreloader;
    BoolOptional blockUi;
    BoolOptional hasChanges;
    node_view::details::NodeViewStatePatch blockPatch;
    node_view::details::NodeViewStatePatch totalsPatch;
};

} // namespace nx::vms::client::desktop
