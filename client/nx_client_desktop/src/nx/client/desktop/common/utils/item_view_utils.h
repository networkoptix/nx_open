#pragma once

#include <QtCore/QModelIndex>

class QAbstractItemView;

namespace nx {
namespace client {
namespace desktop {

class TreeView;

enum class BatchToggleMode
{
    unify,  //< If at least one check box was unchecked, check all, otherwise uncheck all.
    invert  //< Invert each check box state.
};

class ItemViewUtils
{
public:
    /* Inverts check box at specified index. */
    static void toggleCheckBox(QAbstractItemModel* model, const QModelIndex& index);

    static void toggleCheckBox(QAbstractItemView* view,
        const QModelIndex& index, int checkBoxColumn);

    /* Batch-toggles check boxes at selected rows, with specified toggle mode. */
    static void toggleSelectedRows(QAbstractItemView* view, int checkBoxColumn,
        BatchToggleMode toggleMode = BatchToggleMode::unify);

    /* Sets up automatic toggle of a check box when its row is clicked. */
    static void autoToggleOnRowClick(QAbstractItemView* view, int checkBoxColumn,
        Qt::KeyboardModifiers prohibitedKeyboardModifiers = Qt::NoModifier);

    /* Sets up automatic toggle of check boxes at selected rows when Space key is pressed. */
    static void autoToggleOnSpaceKey(TreeView* view, int checkBoxColumn,
        BatchToggleMode toggleMode = BatchToggleMode::unify);

    // Sets up automatic toggle of check boxes when shift-click selection is performed:
    // check boxes in all affected rows are set to the state of originating row check box.
    static void autoToggleOnShiftClick(TreeView* view, int checkBoxColumn);

    // Default setup for automatic check box toggle.
    // Takes view's selection mode into consideration.
    static void setupDefaultAutoToggle(TreeView* view, int checkBoxColumn);

private:
    ItemViewUtils() = default;
};

} // namespace desktop
} // namespace client
} // namespace nx
