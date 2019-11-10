#pragma once

#include <nx/vms/client/desktop/node_view/details/node_view_state.h>

namespace nx::vms::client::desktop {
namespace settings {

struct PoESettingsState
{
    bool showPoEOverBudgetWarning = false;
    bool blockOperations = false;
    node_view::details::NodeViewState blockState;
    node_view::details::NodeViewState totalsState;
};

} // namespace settings
} // namespace nx::vms::client::desktop
