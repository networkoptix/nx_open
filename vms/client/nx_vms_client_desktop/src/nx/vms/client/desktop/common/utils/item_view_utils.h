#pragma once

#include <QtCore/QModelIndex>

class QAbstractItemView;

namespace nx::vms::client::desktop {

enum class BatchToggleMode
{
    unify,  //< If at least one check box was unchecked, check all, otherwise uncheck all.
    invert  //< Invert each check box state.
};

namespace item_view_utils {

using IsCheckable = std::function<bool (const QModelIndex& index)>;

/** Inverts check box at specified index. */
void toggleCheckBox(
    QAbstractItemModel* model,
    const QModelIndex& index,
    IsCheckable isCheckable = {});

void toggleCheckBox(
    QAbstractItemView* view,
    const QModelIndex& index,
    int checkBoxColumn,
    IsCheckable isCheckable = {});

/** Batch-toggles check boxes at selected rows, with specified toggle mode. */
void toggleSelectedRows(
    QAbstractItemView* view,
    int checkBoxColumn,
    BatchToggleMode toggleMode = BatchToggleMode::unify,
    IsCheckable isCheckable = {});

/** Sets up automatic toggle of a check box when its row is clicked. */
void autoToggleOnRowClick(
    QAbstractItemView* view,
    int checkBoxColumn,
    Qt::KeyboardModifiers prohibitedKeyboardModifiers = Qt::NoModifier,
    IsCheckable isCheckable = {});

/** Sets up automatic toggle of check boxes at selected rows when Space key is pressed. */
template <class T>
void autoToggleOnSpaceKey(
    T* view,
    int checkBoxColumn,
    BatchToggleMode toggleMode = BatchToggleMode::unify,
    IsCheckable isCheckable = {});

/** Sets up automatic toggle of check boxes when shift-click selection is performed:
 * check boxes in all affected rows are set to the state of originating row check box. */
template <class T>
void autoToggleOnShiftClick(
    T* view,
    int checkBoxColumn,
    IsCheckable isCheckable = {});

/** Default setup for automatic check box toggle.
 * Takes view's selection mode into consideration. */
template <class T>
void setupDefaultAutoToggle(
    T* view,
    int checkBoxColumn,
    IsCheckable isCheckable = {});

} // namespace item_view_utils
} // namespace nx::vms::client::desktop
