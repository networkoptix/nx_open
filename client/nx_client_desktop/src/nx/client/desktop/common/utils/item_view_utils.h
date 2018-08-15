#pragma once

#include <QtCore/QModelIndex>

class QAbstractItemView;

namespace nx::client::desktop {

class TreeView;

enum class BatchToggleMode
{
    unify,  //< If at least one check box was unchecked, check all, otherwise uncheck all.
    invert  //< Invert each check box state.
};

namespace item_view_utils {

/** Inverts check box at specified index. */
void toggleCheckBox(QAbstractItemModel* model, const QModelIndex& index);

void toggleCheckBox(QAbstractItemView* view, const QModelIndex& index, int checkBoxColumn);

/** Batch-toggles check boxes at selected rows, with specified toggle mode. */
void toggleSelectedRows(QAbstractItemView* view, int checkBoxColumn,
    BatchToggleMode toggleMode = BatchToggleMode::unify);

/** Sets up automatic toggle of a check box when its row is clicked. */
void autoToggleOnRowClick(QAbstractItemView* view, int checkBoxColumn,
    Qt::KeyboardModifiers prohibitedKeyboardModifiers = Qt::NoModifier);

/** Sets up automatic toggle of check boxes at selected rows when Space key is pressed. */
void autoToggleOnSpaceKey(TreeView* view, int checkBoxColumn,
    BatchToggleMode toggleMode = BatchToggleMode::unify);

/** Sets up automatic toggle of check boxes when shift-click selection is performed:
 * check boxes in all affected rows are set to the state of originating row check box. */
void autoToggleOnShiftClick(TreeView* view, int checkBoxColumn);

/** Default setup for automatic check box toggle.
 * Takes view's selection mode into consideration. */
void setupDefaultAutoToggle(TreeView* view, int checkBoxColumn);

} // namespace item_view_utils
} // namespace nx::client::desktop
