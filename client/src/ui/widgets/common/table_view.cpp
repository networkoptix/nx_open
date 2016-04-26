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

bool QnTableView::edit(const QModelIndex& index, EditTrigger trigger, QEvent* event)
{
    if (trigger == QAbstractItemView::SelectedClicked && (this->editTriggers() & QAbstractItemView::DoubleClicked))
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
    return QRect(0, rowViewportPosition(row), width(), rowHeight(row));
}
