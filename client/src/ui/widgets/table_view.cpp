
#include "table_view.h"

#include <QtCore/QEvent>
#include <QtGui/QHeaderView>
#include <QtGui/QMouseEvent>
#include <QtGui/QScrollBar>

#include "client/client_globals.h"


bool ItemViewEventForwarder::eventFilter(QObject *, QEvent *aEvent)
{
    if (aEvent->type() == QEvent::Enter)
        emit entered();
    return false;
}


QnTableView::QnTableView(QWidget *parent) :
    base_type(parent)
{
    horizontalHeader()->installEventFilter(&m_eventForwarder);
    verticalHeader()->installEventFilter(&m_eventForwarder);
    horizontalScrollBar()->installEventFilter(&m_eventForwarder);
    verticalScrollBar()->installEventFilter(&m_eventForwarder);

    connect(
        &m_eventForwarder,
        SIGNAL(entered()),
        this,
        SLOT(resetCursor()));

}


QnTableView::~QnTableView() {
    return;
}

bool QnTableView::edit(const QModelIndex &index, EditTrigger trigger, QEvent *event) {
    if (trigger == QAbstractItemView::SelectedClicked
            && this->editTriggers() & QAbstractItemView::DoubleClicked)
        return base_type::edit(index, QAbstractItemView::DoubleClicked, event);
    return base_type::edit(index, trigger, event);
}

void QnTableView::mouseMoveEvent(QMouseEvent *event)
{
    QAbstractItemModel *m(model());
    // Only do something when a model is set.
    if (isEnabled() && m)
    {
        QModelIndex index = indexAt(event->pos());
        if (index.isValid())
        {
            // When the index is valid, compare it to the last row.
            // Only do something when the the mouse has moved to a new row.
            if (index != m_lastMouseModelIndex)
            {
                m_lastMouseModelIndex = index;
                // Request the data for the MyMouseCursorRole.
                QVariant data = m->data(index, Qn::ItemMouseCursorRole);

                Qt::CursorShape shape = Qt::ArrowCursor;
                if (!data.isNull())
                    shape = static_cast<Qt::CursorShape>(data.toInt());
                setCursor(shape);
            }
        }
        else
        {
            resetCursor();
        }
    }
    QTableView::mouseMoveEvent(event);
}

void QnTableView::resetCursor()
{
    if (m_lastMouseModelIndex.isValid())
        // Set he mouse-cursor to the default when it isn't already.
        setCursor(Qt::ArrowCursor);
    m_lastMouseModelIndex = QModelIndex();
}

void QnTableView::leaveEvent(QEvent * event)
{
    Q_UNUSED(event)
    resetCursor();
}
