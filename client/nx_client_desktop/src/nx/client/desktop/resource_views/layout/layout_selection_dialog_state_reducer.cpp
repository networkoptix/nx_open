#include "layout_selection_dialog_state_reducer.h"

namespace nx {
namespace client {
namespace desktop {

LayoutSelectionDialogStateReducer::State LayoutSelectionDialogStateReducer::setSelection(
    State state,
    const QnUuid& layoutId,
    bool selected)
{
    const auto it = state.layouts.find(layoutId);
    if (it == state.layouts.end())
    {
        NX_EXPECT(false, "No layout with specified id");
        return state;
    }

    auto& layoutData = it->data;
    if (layoutData.checked == selected)
    {
        NX_EXPECT(false, "Layout has specified selection state already");
        return state;
    }

    layoutData.checked = selected;
    return state;
}

} // namespace desktop
} // namespace client
} // namespace nx
