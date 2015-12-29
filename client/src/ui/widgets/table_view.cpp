
#include "table_view.h"

#include <QtCore/QEvent>
#include <QtWidgets/QHeaderView>
#include <QtGui/QMouseEvent>
#include <QtWidgets/QScrollBar>

#include <client/client_globals.h>
#include <utils/common/event_processors.h>

QnTableView::QnTableView(QWidget *parent):
    base_type(parent)
{
    QnSingleEventSignalizer *signalizer = new QnSingleEventSignalizer(this);
    signalizer->setEventType(QEvent::Enter);

    horizontalHeader()->installEventFilter(signalizer);
    verticalHeader()->installEventFilter(signalizer);
    horizontalScrollBar()->installEventFilter(signalizer);
    verticalScrollBar()->installEventFilter(signalizer);

    connect(signalizer, &QnSingleEventSignalizer::activated, this, &QnTableView::resetCursor);
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

void QnTableView::mouseMoveEvent(QMouseEvent *event) {
    QAbstractItemModel *model = this->model();
    // Only do something when a model is set.
    if (isEnabled() && model) {
        QModelIndex index = indexAt(event->pos());
        if (index.isValid()) {
            // When the index is valid, compare it to the last row.
            // Only do something when the the mouse has moved to a new row.
            if (index != m_lastMouseModelIndex)
            {
                m_lastMouseModelIndex = index;
                // Request the data for the MyMouseCursorRole.
                QVariant data = model->data(index, Qn::ItemMouseCursorRole);

                Qt::CursorShape shape = Qt::ArrowCursor;
                if (!data.isNull())
                    shape = static_cast<Qt::CursorShape>(data.toInt());
                setCursor(shape);
            }
        } else {
            resetCursor();
        }
    }

    QTableView::mouseMoveEvent(event);
}

void QnTableView::resetCursor() {
    if (m_lastMouseModelIndex.isValid()) // Set the mouse-cursor to the default when it isn't already.
        setCursor(Qt::ArrowCursor);

    m_lastMouseModelIndex = QModelIndex();
}

void QnTableView::leaveEvent(QEvent *) {
    resetCursor();
}

// ------------------------------------------------QnCheckableTableView ---------------------

void QnCheckableTableView::mousePressEvent(QMouseEvent *event)
{
    if (isEnabled() && this->model()) {
        QModelIndex index = indexAt(event->pos());
        if (index.column() == 0) {
            auto modifiers = event->modifiers();
            if (!(modifiers & Qt::ShiftModifier))
                modifiers |= Qt::ControlModifier;
            QMouseEvent updatedEvent(event->type(), event->localPos(), event->button(), event->buttons(), modifiers);
            base_type::mousePressEvent(&updatedEvent);
            return;
        }
    }
    base_type::mousePressEvent(event);
}

