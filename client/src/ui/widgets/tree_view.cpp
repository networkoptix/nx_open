#include "tree_view.h"
#include <QTimerEvent>
#include <QDragMoveEvent>

QnTreeView::QnTreeView(QWidget *parent): 
    QTreeView(parent)
{}

QnTreeView::~QnTreeView() {
    return;
}

void QnTreeView::keyPressEvent(QKeyEvent *event) {
    if(event->key() == Qt::Key_Enter || event->key() == Qt::Key_Return) {
        if (state() != EditingState || hasFocus()) {
            if (currentIndex().isValid())
                emit enterPressed(currentIndex());
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

QString QnTreeView::toolTipAt(const QPointF &pos) const {
    QVariant toolTip = indexAt(pos.toPoint()).data(Qt::ToolTipRole);
    if (toolTip.canConvert<QString>()) {
        return toolTip.toString();
    } else {
        return QString();
    }
}
