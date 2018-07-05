#include "item_view_utils.h"

#include <algorithm>

#include <QtGui/QMouseEvent>
#include <QtCore/QScopedPointer>
#include <QtWidgets/QAbstractItemView>
#include <QtWidgets/QApplication>

#include <nx/client/desktop/common/widgets/tree_view.h>
#include <nx/utils/log/assert.h>

namespace nx {
namespace client {
namespace desktop {

void ItemViewUtils::toggleCheckBox(
    QAbstractItemModel* model,
    const QModelIndex& index,
    const CheckableCheckFunction& checkableCheck)
{
    if (checkableCheck && !checkableCheck(index))
        return;

    NX_ASSERT(model);
    const auto oldValue = index.data(Qt::CheckStateRole).toInt();
    const int newValue = oldValue != Qt::Checked ? Qt::Checked : Qt::Unchecked;
    model->setData(index, newValue, Qt::CheckStateRole);
}

void ItemViewUtils::toggleCheckBox(
    QAbstractItemView* view,
    const QModelIndex& index,
    int checkBoxColumn,
    const CheckableCheckFunction& checkableCheck)
{
    NX_ASSERT(view);
    toggleCheckBox(view->model(), index.sibling(index.row(), checkBoxColumn), checkableCheck );
}

void ItemViewUtils::toggleSelectedRows(
    QAbstractItemView* view,
    int checkBoxColumn,
    BatchToggleMode toggleMode,
    const CheckableCheckFunction& checkableCheck)
{
    NX_ASSERT(view);
    const auto selectedRows = view->selectionModel()->selectedRows(checkBoxColumn);
    auto model = view->model();

    switch (toggleMode)
    {
        case BatchToggleMode::unify:
        {
            const bool anyUnchecked = std::any_of(selectedRows.cbegin(), selectedRows.cend(),
                [](const QModelIndex& index)
                {
                    return index.data(Qt::CheckStateRole).toInt() != Qt::Checked;
                });

            const int newValue = anyUnchecked ? Qt::Checked : Qt::Unchecked;

            for (const auto& index: selectedRows)
            {
                if (checkableCheck && checkableCheck(index))
                    model->setData(index, newValue, Qt::CheckStateRole);
            }

            break;
        }

        case BatchToggleMode::invert:
        {
            for (const auto& index: selectedRows)
                toggleCheckBox(model, index, checkableCheck);

            break;
        }
    }
}

void ItemViewUtils::autoToggleOnRowClick(
    QAbstractItemView* view,
    int checkBoxColumn,
    Qt::KeyboardModifiers prohibitedKeyboardModifiers,
    const CheckableCheckFunction& checkableCheck)
{
    NX_ASSERT(view);

    QObject::connect(view, &QAbstractItemView::clicked,
        [checkableCheck, view, checkBoxColumn, prohibitedKeyboardModifiers](const QModelIndex& index)
        {
            if (prohibitedKeyboardModifiers == Qt::NoModifier
                || (qApp->keyboardModifiers() & prohibitedKeyboardModifiers) == 0)
            {
                toggleCheckBox(view, index, checkBoxColumn, checkableCheck);
            }
        });
}

void ItemViewUtils::autoToggleOnSpaceKey(
    TreeView* view,
    int checkBoxColumn,
    BatchToggleMode toggleMode,
    const CheckableCheckFunction& checkableCheck)
{
    NX_ASSERT(view);
    view->setIgnoreDefaultSpace(true);

    QObject::connect(view, &TreeView::spacePressed,
        [view, checkBoxColumn, toggleMode, checkableCheck]()
        {
            toggleSelectedRows(view, checkBoxColumn, toggleMode, checkableCheck);
        });
}

void ItemViewUtils::autoToggleOnShiftClick(
    TreeView* view,
    int checkBoxColumn,
    const CheckableCheckFunction& checkableCheck)
{
    NX_ASSERT(view);

    QObject::connect(view, &TreeView::selectionChanging,
        [checkableCheck, view, checkBoxColumn](
            QItemSelectionModel::SelectionFlags selectionFlags,
            const QModelIndex& index,
            const QEvent* event)
        {
            if (checkableCheck && !checkableCheck(index))
                return;

            const bool specialHandling = event && event->type() == QEvent::MouseButtonPress
                && static_cast<const QMouseEvent*>(event)->modifiers().testFlag(Qt::ShiftModifier);

            const auto current = view->currentIndex(); //< Originating item.

            if (!specialHandling || !current.isValid() || !index.isValid()
                || !selectionFlags.testFlag(QItemSelectionModel::Select))
            {
                return;
            }

            const auto newCheckValue = current.sibling(current.row(), checkBoxColumn)
                .data(Qt::CheckStateRole);

            QAbstractItemModel* model = view->model();

            QPair<int, int> range(index.row(), current.row());
            if (range.first > range.second)
                qSwap(range.first, range.second);

            for (int i = range.first; i <= range.second; ++i)
            {
                model->setData(index.sibling(i, checkBoxColumn),
                    newCheckValue, Qt::CheckStateRole);
            }
        });
}

void ItemViewUtils::setupDefaultAutoToggle(
    TreeView* view,
    int checkBoxColumn,
    const CheckableCheckFunction& checkableCheck)
{
    NX_ASSERT(view);
    Qt::KeyboardModifiers prohibitedModifiers = Qt::NoModifier;

    switch (view->selectionMode())
    {
        case QAbstractItemView::ContiguousSelection:
            prohibitedModifiers = Qt::ShiftModifier;
            break;

        case QAbstractItemView::ExtendedSelection:
            prohibitedModifiers = Qt::ShiftModifier | Qt::ControlModifier;
            break;

        default:
            break;
    }

    autoToggleOnRowClick(view, checkBoxColumn, prohibitedModifiers, checkableCheck);
    autoToggleOnSpaceKey(view, checkBoxColumn, BatchToggleMode::unify, checkableCheck);

    if (prohibitedModifiers.testFlag(Qt::ShiftModifier))
        autoToggleOnShiftClick(view, checkBoxColumn, checkableCheck);
}

} // namespace desktop
} // namespace client
} // namespace nx
