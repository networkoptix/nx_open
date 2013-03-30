#include "tree_view.h"

#include <QtCore/QTimerEvent>
#include <QtGui/QDragMoveEvent>

#include <utils/common/variant.h>

#include <client/client_globals.h>
#include <ui/help/help_topic_accessor.h>

QnTreeView::QnTreeView(QWidget *parent): 
    base_type(parent)
{}

QnTreeView::~QnTreeView() {
    return;
}

int QnTreeView::rowHeight(const QModelIndex &index) const {
    return base_type::rowHeight(index);
}

void QnTreeView::keyPressEvent(QKeyEvent *event) {
    if(event->key() == Qt::Key_Enter || event->key() == Qt::Key_Return) {
        if (state() != EditingState || hasFocus()) {
            if (currentIndex().isValid())
                emit enterPressed(currentIndex());
            event->ignore();
        }
    }
    if(event->key() == Qt::Key_Space) {
        if (state() != EditingState || hasFocus()) {
            if (currentIndex().isValid())
                emit spacePressed(currentIndex());
            event->ignore();
        }
    }

    QTreeView::keyPressEvent(event);
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
            /* Open the node that the mouse is hovered over. 
             * Don't close it if it's already opened as the default implementation does. */
            QModelIndex index = indexAt(pos);
            setExpanded(index, true);
        }
        m_openTimer.stop();
    }

    QTreeView::timerEvent(event);
}

