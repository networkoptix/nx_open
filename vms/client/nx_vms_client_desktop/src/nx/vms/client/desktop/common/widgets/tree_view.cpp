// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "tree_view.h"

#include <QtCore/QScopedValueRollback>
#include <QtCore/QTimerEvent>
#include <QtGui/QDragMoveEvent>
#include <QtGui/QWheelEvent>
#include <QtWidgets/QApplication>
#include <QtWidgets/QScrollBar>

#include <nx/utils/scope_guard.h>
#include <nx/vms/client/desktop/common/models/item_model_algorithm.h>
#include <nx/vms/client/desktop/help/help_topic_accessor.h>
#include <utils/common/variant.h>

namespace nx::vms::client::desktop {

TreeView::TreeView(QWidget* parent): base_type(parent)
{
    setDragDropOverwriteMode(true);
}

void TreeView::scrollContentsBy(int dx, int dy)
{
    base_type::scrollContentsBy(dx, dy);

    /* Workaround for editor staying open when a scroll by wheel
     * (from either view itself or it's scrollbar) is performed: */
    if (state() == EditingState)
        currentChanged(currentIndex(), currentIndex());
}

bool TreeView::edit(const QModelIndex& index, QAbstractItemView::EditTrigger trigger, QEvent* event)
{
    if (!m_allowMultiSelectionEdit && selectedIndexes().size() > 1)
        return false;
    return base_type::edit(index, trigger, event);
}

void TreeView::keyPressEvent(QKeyEvent* event)
{
    if (state() == EditingState || !currentIndex().isValid())
    {
        base_type::keyPressEvent(event);
        return;
    }

    switch (event->key())
    {
        case Qt::Key_Enter:
        case Qt::Key_Return:
            event->ignore();
            if (hasFocus())
                emit activated(currentIndex());
            emit enterPressed(currentIndex());
            return; //< Don't call base implementation which triggers edit on macOS.

        case Qt::Key_Space:
            event->ignore();
            emit spacePressed(currentIndex());
            if (m_isDefauldSpacePressIgnored)
                return;
            break;

        case Qt::Key_F2:
            if (!edit(currentIndex(), EditKeyPressed, event))
                event->ignore();
            return; //< Don't call base implementation which triggers edit only on Windows.

        case Qt::Key_Left:
        case Qt::Key_Right:
        case Qt::Key_Plus:
        case Qt::Key_Minus:
            if (QApplication::keyboardModifiers() == Qt::AltModifier && itemsExpandable())
            {
                const bool isExpandAction =
                    event->key() == Qt::Key_Right || event->key() == Qt::Key_Plus;
                setExpandedRecursively(currentIndex(), isExpandAction);
                return;
            }
            break;

        default:
            break;
    }

    base_type::keyPressEvent(event);
}

void TreeView::dragMoveEvent(QDragMoveEvent* event)
{
    if (autoExpandDelay() >= 0)
    {
        m_dragMovePos = event->pos();
        m_openTimer.start(autoExpandDelay(), this);
    }

    /* Important! Skip QTreeView's implementation. */
    QScopedValueRollback<bool> guard(m_inDragDropEvent, true);
    QAbstractItemView::dragMoveEvent(event);
}

void TreeView::dragLeaveEvent(QDragLeaveEvent* event)
{
    m_openTimer.stop();
    base_type::dragLeaveEvent(event);
}

void TreeView::dropEvent(QDropEvent* event)
{
    QScopedValueRollback<bool> guard(m_inDragDropEvent, true);
    base_type::dropEvent(event);
}

void TreeView::timerEvent(QTimerEvent* event)
{
    if (event->timerId() == m_openTimer.timerId())
    {
        QPoint pos = m_dragMovePos;
        if (state() == QAbstractItemView::DraggingState && viewport()->rect().contains(pos))
        {
            /* Open the node that the mouse is hovered over.
             * Don't close it if it's already opened as the default implementation does. */
            QModelIndex index = indexAt(pos);
            setExpanded(index, true);
        }
        m_openTimer.stop();
    }

    base_type::timerEvent(event);
}

void TreeView::mouseDoubleClickEvent(QMouseEvent* event)
{
    auto expandsOnDoubleClickGuard = nx::utils::makeScopeGuard(
        [this, previosValue = expandsOnDoubleClick()]()
        {
            setExpandsOnDoubleClick(previosValue);
        });

    // Delegate can occasionally change the model, so keeping persistent index
    const QPersistentModelIndex persistent = indexAt(event->pos());
    if (m_confirmExpand && !m_confirmExpand(persistent))
        setExpandsOnDoubleClick(false);

    if (itemsExpandable()
        && expandsOnDoubleClick()
        && QApplication::keyboardModifiers() == Qt::AltModifier)
    {
        const auto clickedIndex = indexAt(event->pos());
        if (clickedIndex.isValid())
        {
            setExpandedRecursively(clickedIndex, isExpanded(clickedIndex));
            return;
        }
    }

    base_type::mouseDoubleClickEvent(event);
}

void TreeView::wheelEvent(QWheelEvent* event)
{
    if (m_ignoreWheelEvents)
        event->ignore();
    else
        base_type::wheelEvent(event);
}

bool TreeView::isDefaultSpacePressIgnored() const
{
    return m_isDefauldSpacePressIgnored;
}

void TreeView::setDefaultSpacePressIgnored(bool isIgnored)
{
    m_isDefauldSpacePressIgnored = isIgnored;
}

void TreeView::setMultiSelectionEditAllowed(bool value)
{
    m_allowMultiSelectionEdit = value;
}

bool TreeView::isMultiSelectionEditAllowed() const
{
    return m_allowMultiSelectionEdit;
}

bool TreeView::dropOnBranchesAllowed() const
{
    return m_dropOnBranchesAllowed;
}

void TreeView::setDropOnBranchesAllowed(bool value)
{
    m_dropOnBranchesAllowed = value;
}

QRect TreeView::visualRect(const QModelIndex& index) const
{
    QRect result = base_type::visualRect(index);
    if (!m_inDragDropEvent)
        return result;

    if (m_dropOnBranchesAllowed && index.column() == treePosition())
        result.setLeft(columnViewportPosition(index.column()));

    return result;
}

void TreeView::setConfirmExpandDelegate(ConfirmExpandDelegate value)
{
    m_confirmExpand = value;
}

void TreeView::setIgnoreWheelEvents(bool ignore)
{
    m_ignoreWheelEvents = ignore;
}

void TreeView::collapseRecursively(const QModelIndex& index)
{
    if (!NX_ASSERT(index.isValid(), "Invalid index"))
        return;

    if (!NX_ASSERT(index.model() == model(), "Invalid index: wrong model"))
        return;

    const auto nonLeafIndexes = item_model::getNonLeafIndexes(model(), index);
    collapse(index);
    for (const auto& index: nonLeafIndexes)
        collapse(index);
}

void TreeView::setExpandedRecursively(const QModelIndex& index, bool expanded)
{
    if (expanded)
        expandRecursively(index);
    else
        collapseRecursively(index);
}

int TreeView::calculateHeight(const QModelIndex& index) const
{
    int height{0};
    if (index.isValid())
        height += rowHeight(index);

    if (!model()->hasChildren(index))
        return height;

    auto rowCount = model()->rowCount(index);
    for (int i = 0; i < rowCount; ++i)
        height += calculateHeight(model()->index(i, 0, index));

    return height;
}

QSize TreeView::sizeHint() const
{
    auto defaultSizeHint = QTreeView::sizeHint();
    if (!m_useCustomSizeHint)
        return defaultSizeHint;

    return QSize(defaultSizeHint.width(), calculateHeight(QModelIndex()));
}

void TreeView::setCustomSizeHint(bool isUsed)
{
    m_useCustomSizeHint = isUsed;
}

QItemSelectionModel::SelectionFlags TreeView::selectionCommand(
    const QModelIndex& index, const QEvent* event) const
{
    const auto result = base_type::selectionCommand(index, event);
    emit selectionChanging(result, index, event);
    return result;
}

} // namespace nx::vms::client::desktop
