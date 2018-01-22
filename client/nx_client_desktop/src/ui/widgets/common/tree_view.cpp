#include "tree_view.h"

#include <QtCore/QTimerEvent>
#include <QtCore/QScopedValueRollback>
#include <QtGui/QDragMoveEvent>

#include <QtWidgets/QScrollBar>

#include <utils/common/variant.h>

#include <client/client_globals.h>
#include <ui/help/help_topic_accessor.h>


QnTreeView::QnTreeView(QWidget *parent):
    base_type(parent),
    m_ignoreDefaultSpace(false),
    m_dropOnBranchesAllowed(true),
    m_inDragDropEvent(false)
{
    setDragDropOverwriteMode(true);

    verticalScrollBar()->installEventFilter(this);
    handleVerticalScrollbarVisibilityChanged();
}

QnTreeView::~QnTreeView()
{
}

void QnTreeView::scrollContentsBy(int dx, int dy)
{
    base_type::scrollContentsBy(dx, dy);

    /* Workaround for editor staying open when a scroll by wheel
     * (from either view itself or it's scrollbar) is performed: */
    if (state() == EditingState)
        currentChanged(currentIndex(), currentIndex());
}

void QnTreeView::keyPressEvent(QKeyEvent* event)
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
                if (m_ignoreDefaultSpace)
                    return;
            }
        }
    }

    base_type::keyPressEvent(event);
}

bool QnTreeView::eventFilter(QObject* object, QEvent* event)
{
    if ((event->type() == QEvent::Show || event->type() == QEvent::Hide)
        && object == verticalScrollBar())
    {
        handleVerticalScrollbarVisibilityChanged();
    }

    return base_type::eventFilter(object, event);
}

void QnTreeView::handleVerticalScrollbarVisibilityChanged()
{
    const auto scrollBar = verticalScrollBar();
    const bool visible = scrollBar && scrollBar->isVisible();
    if (visible == m_verticalScrollBarVisible)
        return;

    m_verticalScrollBarVisible = visible;
    emit verticalScrollbarVisibilityChanged();
}

bool QnTreeView::verticalScrollBarIsVisible() const
{
    return m_verticalScrollBarVisible;
}

void QnTreeView::dragMoveEvent(QDragMoveEvent* event)
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

void QnTreeView::dragLeaveEvent(QDragLeaveEvent* event)
{
    m_openTimer.stop();
    base_type::dragLeaveEvent(event);
}

void QnTreeView::dropEvent(QDropEvent* event)
{
    QScopedValueRollback<bool> guard(m_inDragDropEvent, true);
    base_type::dropEvent(event);
}

void QnTreeView::timerEvent(QTimerEvent* event)
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

void QnTreeView::mouseDoubleClickEvent(QMouseEvent* event)
{
    // Delegate can occasionally change the model, so keeping persistent index
    const QPersistentModelIndex persistent = indexAt(event->pos());
    if (!m_confirmExpand || m_confirmExpand(persistent))
        base_type::mouseDoubleClickEvent(event);
    else if (persistent.isValid())
        emit doubleClicked(persistent);
}

QSize QnTreeView::viewportSizeHint() const
{
    /*
     * Fix for Qt 5.6 bug: viewportSizeHint() returns size hint for visible area only.
     */
    #if QT_VERSION < 0x050600 && QT_VERSION > 0x050602
    #error Check if this workaround is required in current Qt version
    #endif
    return base_type::viewportSizeHint() + QSize(horizontalOffset(), verticalOffset());
}

bool QnTreeView::ignoreDefaultSpace() const
{
    return m_ignoreDefaultSpace;
}

void QnTreeView::setIgnoreDefaultSpace(bool value)
{
    m_ignoreDefaultSpace = value;
}

bool QnTreeView::dropOnBranchesAllowed() const
{
    return m_dropOnBranchesAllowed;
}

void QnTreeView::setDropOnBranchesAllowed(bool value)
{
    m_dropOnBranchesAllowed = value;
}

QRect QnTreeView::visualRect(const QModelIndex& index) const
{
    QRect result = base_type::visualRect(index);
    if (!m_inDragDropEvent)
        return result;

    if (m_dropOnBranchesAllowed && index.column() == treePosition())
        result.setLeft(columnViewportPosition(index.column()));

    return result;
}

void QnTreeView::setConfirmExpandDelegate(ConfirmExpandDelegate value)
{
    m_confirmExpand = value;
}

QItemSelectionModel::SelectionFlags QnTreeView::selectionCommand(
    const QModelIndex& index, const QEvent* event) const
{
    const auto result = base_type::selectionCommand(index, event);
    emit selectionChanging(result, index, event);
    return result;
}
