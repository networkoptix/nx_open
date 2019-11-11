#pragma once

#include <nx/vms/client/desktop/node_view/details/node_view_state_patch.h>

#include <nx/utils/std/optional.h>

namespace nx::vms::client::desktop {
namespace settings {

struct PoESettingsState
{
    bool showPoEOverBudgetWarning = false;
    bool blockOperations = false;
    bool hasChanges = false;
};

struct PoESettingsStatePatch
{
    using BoolOptional = std::optional<bool>;
    BoolOptional showPoEOverBudgetWarning;
    BoolOptional blockOperations;
    BoolOptional hasChanges = false;
    node_view::details::NodeViewStatePatch blockPatch;
    node_view::details::NodeViewStatePatch totalsPatch;
};

} // namespace settings
} // namespace nx::vms::client::desktop
