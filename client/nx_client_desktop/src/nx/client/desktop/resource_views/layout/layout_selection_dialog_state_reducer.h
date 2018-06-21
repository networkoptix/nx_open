#pragma once

#include <nx/client/desktop/resource_views/layout/layout_selection_dialog_state.h>

namespace nx {
namespace client {
namespace desktop {

class LayoutSelectionDialogStateReducer
{
public:
    using State = LayoutSelectionDialogState;

    static State setSelection(
        State state,
        const QnUuid& layoutId,
        bool selected);

private:
    LayoutSelectionDialogStateReducer() = default;
};

} // namespace desktop
} // namespace client
} // namespace nx

