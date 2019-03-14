#include "tree_view.h"

#include <QtCore/QTimerEvent>
#include <QtCore/QScopedValueRollback>
#include <QtGui/QDragMoveEvent>
#include <QtWidgets/QScrollBar>

#include <ui/help/help_topic_accessor.h>
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

void TreeView::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Enter || event->key() == Qt::Key_Return)
    {
        bool canActivate = state() != EditingState;
        if (canActivate)
        {
            event->ignore();
            if (currentIndex().isValid())
                emit enterPressed(currentIndex());
        }
    }

    if (event->key() == Qt::Key_Space)
    {
        if (state() != EditingState)
        {
            event->ignore();
            if (currentIndex().isValid())
            {
                emit spacePressed(currentIndex());
                if (m_isDefauldSpacePressIgnored)
                    return;
            }
        }
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

    base_type::mouseDoubleClickEvent(event);
}

QSize TreeView::viewportSizeHint() const
{
    /*
     * Fix for Qt 5.6 bug: viewportSizeHint() returns size hint for visible area only.
     */
    #if QT_VERSION < 0x050600 && QT_VERSION > 0x050602
    #error Check if this workaround is required in current Qt version
    #endif
    return base_type::viewportSizeHint() + QSize(horizontalOffset(), verticalOffset());
}

bool TreeView::isDefaultSpacePressIgnored() const
{
    return m_isDefauldSpacePressIgnored;
}

void TreeView::setDefaultSpacePressIgnored(bool isIgnored)
{
    m_isDefauldSpacePressIgnored = isIgnored;
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

QItemSelectionModel::SelectionFlags TreeView::selectionCommand(
    const QModelIndex& index, const QEvent* event) const
{
    const auto result = base_type::selectionCommand(index, event);
    emit selectionChanging(result, index, event);
    return result;
}

} // namespace nx::vms::client::desktop
