#include "tree_view.h"

#include <QtCore/QTimerEvent>
#include <QtGui/QDragMoveEvent>

#include <utils/common/variant.h>

#include <client/client_globals.h>
#include <ui/help/help_topic_accessor.h>

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
    if (toolTip.convert(QVariant::String)) {
        return toolTip.toString();
    } else {
        return QString();
    }
}

int QnTreeView::helpTopicAt(const QPointF &pos) const {
    int result = -1;

    QModelIndex index = indexAt(pos.toPoint());
    if(index.isValid())
        result = qvariant_cast<int>(index.data(Qn::HelpTopicIdRole), -1);

    /* Get model's help topic if it is not set on the item. */
    if(result == -1 && model())
        result = helpTopic(model());

    return result;
}
