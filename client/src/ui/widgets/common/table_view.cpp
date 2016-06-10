#include "table_view.h"
#include <ui/style/helper.h>

QnTableView::QnTableView(QWidget* parent): base_type(parent)
{
    setMouseTracking(true);
    setAttribute(Qt::WA_Hover, true);
}

QnTableView::~QnTableView()
{
}

QSize QnTableView::viewportSizeHint() const
{
    /* Fix for QTableView bug
     *  QTableView adjusts sizeHint if scrollBar.isVisible()
     *  But isVisible() deals with global visibility and returns false if QTableView is hidden (while its scrollbar is not)
     *  To check local visibility isHidden() must be used
     *  So as a workaround we adjust sizeHint after Qt if !scrollbar.isVisible() && !scrollbar.isHidden()
     */

    QSize size = base_type::viewportSizeHint();

    if (!verticalScrollBar()->isVisible() && !verticalScrollBar()->isHidden())
        size.setWidth(size.width() + verticalScrollBar()->width());

    if (!horizontalScrollBar()->isVisible() && !horizontalScrollBar()->isHidden())
        size.setHeight(size.height() + horizontalScrollBar()->height());

    return size;
}

bool QnTableView::edit(const QModelIndex& index, EditTrigger trigger, QEvent* event)
{
    if (trigger == QAbstractItemView::SelectedClicked && this->editTriggers().testFlag(QAbstractItemView::DoubleClicked))
        return base_type::edit(index, QAbstractItemView::DoubleClicked, event);

    return base_type::edit(index, trigger, event);
}

bool QnTableView::event(QEvent* event)
{
    /* Call inherited handler first: */
    bool result = base_type::event(event);

    /* Then handle hover events: */
    switch (event->type())
    {
    case QEvent::HoverEnter:
    case QEvent::HoverLeave:
    case QEvent::HoverMove:
        hoverEvent(static_cast<QHoverEvent*>(event));
        break;
    };

    return result;
}

void QnTableView::hoverEvent(QHoverEvent* event)
{
    if (!viewport())
        return;

    static const QModelIndex kNoHover;
    if (isEnabled() && model())
    {
        switch (event->type())
        {
        case QEvent::HoverLeave:
            changeHover(kNoHover);
            break;

        case QEvent::HoverEnter:
        case QEvent::HoverMove:
            changeHover(childAt(event->pos()) == viewport() ? indexAt(viewport()->mapFrom(this, event->pos())) : kNoHover);
            break;
        }
    }
    else
    {
        changeHover(kNoHover);
    }
}

void QnTableView::changeHover(const QModelIndex& index)
{
    bool selectRows = selectionBehavior() == SelectRows;
    if (!selectRows)
        setProperty(style::Properties::kHoveredRowProperty, -1);

    if (index != m_lastModelIndex)
    {
        bool rowChange = selectRows && index.row() != m_lastModelIndex.row();

        /* Emit leave signals and mark row rectangle for repaint: */
        if (m_lastModelIndex.isValid())
        {
            if (rowChange)
            {
                viewport()->update(rowRect(m_lastModelIndex.row()));
                emit rowLeave(m_lastModelIndex.row());
            }

            emit hoverLeave(m_lastModelIndex);
        }

        m_lastModelIndex = index;

        if (index.isValid())
        {
            /* Try to obtain cursor from the model, defaulting to zero (Qt::ArrowCursor): */
            QVariant data = model()->data(index, Qn::ItemMouseCursorRole);
            Qt::CursorShape shape = static_cast<Qt::CursorShape>(data.toInt());
            setCursor(shape);

            /* Set current row index property, if needed: */
            if (rowChange)
                setProperty(style::Properties::kHoveredRowProperty, m_lastModelIndex.row());

            /* Emit enter signals and mark row rectangle for repaint: */
            emit hoverEnter(m_lastModelIndex);

            if (rowChange)
            {
                viewport()->update(rowRect(m_lastModelIndex.row()));
                emit rowEnter(m_lastModelIndex.row());
            }
        }
        else
        {
            /* Set default cursor: */
            setCursor(Qt::ArrowCursor);

            /* Clear current row index property: */
            if (rowChange)
                setProperty(style::Properties::kHoveredRowProperty, -1);
        }
    }
}

QRect QnTableView::rowRect(int row) const
{
    int lastColumn = horizontalHeader()->count() - 1;
    int rowStart = columnViewportPosition(0);
    int rowWidth = columnViewportPosition(lastColumn) + columnWidth(lastColumn) - rowStart;
    return QRect(rowStart, rowViewportPosition(row), rowWidth, rowHeight(row));
}

void QnTableView::paintEvent(QPaintEvent* event)
{
    if (selectionBehavior() == SelectRows)
    {
        QModelIndex current = currentIndex();
        if ((hasFocus() || viewport()->hasFocus()) && current.isValid())
        {
            /* Temporarily unfocus current item without sending any notifications: */
            QSignalBlocker blocker(selectionModel());
            selectionModel()->setCurrentIndex(QModelIndex(), QItemSelectionModel::NoUpdate);
            /* Paint table: */
            base_type::paintEvent(event);
            /* Refocus previously focused item (still without sending any notifications): */
            selectionModel()->setCurrentIndex(current, QItemSelectionModel::NoUpdate);

            /* Paint entire row focus frame: */
            QPainter painter(viewport());
            QStyleOptionFocusRect frameOption;
            frameOption.initFrom(this);
            frameOption.rect = rowRect(current.row());
            style()->drawPrimitive(QStyle::PE_FrameFocusRect, &frameOption, &painter, this);

            /* Finish: */
            return;
        }
    }

    /* Default table painting: */
    base_type::paintEvent(event);
}

void QnTableView::currentChanged(const QModelIndex& current, const QModelIndex& previous)
{
    base_type::currentChanged(current, previous);

    /* Since we draw entire row focus rect if selection mode is SelectRows,
     * we need to post update events for rows that lose and gain focus: */
    if (selectionBehavior() == SelectRows && (hasFocus() || viewport()->hasFocus()))
    {
        int previousRow = previous.isValid() ? previous.row() : -1;
        int currentRow = current.isValid() ? current.row() : -1;

        if (previousRow != currentRow)
        {
            if (previousRow >= 0)
                viewport()->update(rowRect(previous.row()));

            if (currentRow >= 0)
                viewport()->update(rowRect(current.row()));
        }
    }
}
