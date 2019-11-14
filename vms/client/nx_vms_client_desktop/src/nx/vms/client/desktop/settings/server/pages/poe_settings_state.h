#pragma once

#include <nx/vms/client/desktop/node_view/details/node_view_state_patch.h>

#include <nx/utils/std/optional.h>

namespace nx::vms::client::desktop {
namespace settings {

struct PoESettingsState
{
    bool autoUpdates = true;
    bool showPreloader = true;
    bool showPoEOverBudgetWarning = false;
    bool blockUi = false;
    bool hasChanges = false;
};

struct PoESettingsStatePatch
{
    using BoolOptional = std::optional<bool>;
    BoolOptional autoUpdates;
    BoolOptional showPoEOverBudgetWarning;
    BoolOptional showPreloader;
    BoolOptional blockUi;
    BoolOptional hasChanges;
    node_view::details::NodeViewStatePatch blockPatch;
    node_view::details::NodeViewStatePatch totalsPatch;
};

} // namespace settings
} // namespace nx::vms::client::desktop
