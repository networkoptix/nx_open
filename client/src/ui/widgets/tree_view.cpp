#include "tree_view.h"
#include <QTimerEvent>
#include <QDragMoveEvent>

QnTreeView::QnTreeView(QWidget *parent): 
    QTreeView(parent)
{}

QnTreeView::~QnTreeView() {
    return;
}

void QnTreeView::dragMoveEvent(QDragMoveEvent *event) {
    if (autoExpandDelay() >= 0) {
        m_dragMovePos = event->pos();
        m_openTimer.start(autoExpandDelay(), this);
    }

    /* Important! Skip QTreeView's implementation. */
    QAbstractItemView::dragMoveEvent(event);
}

void QnTreeView::dragLeaveEvent(QDragLeaveEvent *event) {
    m_openTimer.stop();

    QTreeView::dragLeaveEvent(event);
}

void QnTreeView::timerEvent(QTimerEvent *event) {
    if (event->timerId() == m_openTimer.timerId()) {
        QPoint pos = m_dragMovePos;
        if (state() == QAbstractItemView::DraggingState && viewport()->rect().contains(pos)) {
            QModelIndex index = indexAt(pos);
            setExpanded(index, true);
        }
        m_openTimer.stop();
    }

    QTreeView::timerEvent(event);
}
