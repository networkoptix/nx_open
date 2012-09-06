#include "emulated_frame_widget.h"

#include <QtGui/QMouseEvent>

#include <ui/common/frame_section.h>

namespace {

    const int cursorUpdateTimeoutMSec = 30;

} // anonymous namespace


QnEmulatedFrameWidget::QnEmulatedFrameWidget(QWidget *parent, Qt::WindowFlags windowFlags):
    QWidget(parent, windowFlags)
{
    m_dragProcessor = new DragProcessor(this);
    m_dragProcessor->setHandler(this);

    setAttribute(Qt::WA_Hover, true);
}

QnEmulatedFrameWidget::~QnEmulatedFrameWidget() {
    return;
}

bool QnEmulatedFrameWidget::event(QEvent *event) {
    bool result = base_type::event(event);

    switch(event->type()) {
    case QEvent::HoverEnter:
    case QEvent::HoverMove:
        updateCursor(static_cast<QHoverEvent *>(event)->pos());
        event->accept();
        return true;
    case QEvent::HoverLeave:
        updateCursor(QPoint(-1, -1));
        event->accept();
        return true;
    case QEvent::Timer: {
        QTimerEvent *timerEvent = static_cast<QTimerEvent *>(event);
        if(timerEvent->timerId() != m_timer.timerId())
            break;

        updateCursor();
        event->accept();
        return true;
    }
    default:
        break;
    }

    return result;
}

void QnEmulatedFrameWidget::mousePressEvent(QMouseEvent *event) {
    base_type::mousePressEvent(event);

    Qt::WindowFrameSection section = windowFrameSectionAt(event->pos());
    if(section != Qt::NoSection) {
        m_section = section;
        m_startPinPoint = Qn::calculatePinPoint(geometry(), section);
        m_startSize = size();
        m_dragProcessor->widgetMousePressEvent(this, event);
    }
}

void QnEmulatedFrameWidget::mouseMoveEvent(QMouseEvent *event) {
    base_type::mouseMoveEvent(event);

    m_dragProcessor->widgetMouseMoveEvent(this, event);
}

void QnEmulatedFrameWidget::mouseReleaseEvent(QMouseEvent *event) {
    base_type::mouseReleaseEvent(event);

    m_dragProcessor->widgetMouseReleaseEvent(this, event);
}

void QnEmulatedFrameWidget::startDragProcess(DragInfo *) {
    m_timer.stop();
}

void QnEmulatedFrameWidget::dragMove(DragInfo *info) {
    if(m_section == Qt::TitleBarArea) {
        move(pos() + info->mouseScreenPos() - info->lastMouseScreenPos());
    } else {
        resize(m_startSize + Qn::calculateSizeDelta(info->mouseScreenPos() - info->mousePressScreenPos(), m_section));
        move(pos() + m_startPinPoint - Qn::calculatePinPoint(geometry(), m_section));
    }
}

void QnEmulatedFrameWidget::finishDragProcess(DragInfo *) {
    updateCursor();
}

void QnEmulatedFrameWidget::updateCursor() {
    updateCursor(mapFromGlobal(QCursor::pos()));
}

void QnEmulatedFrameWidget::updateCursor(const QPoint &mousePos) {
    if(!m_dragProcessor->isWaiting())
        return;

    if(mousePos == m_lastMousePos)
        return;

    Qt::CursorShape shape = childAt(mousePos) ? Qt::ArrowCursor : Qn::calculateHoverCursorShape(windowFrameSectionAt(mousePos));
    if(shape == Qt::ArrowCursor) {
        m_timer.stop();
    } else if(!m_timer.isActive()) {
        m_timer.start(cursorUpdateTimeoutMSec, this);
    }
 
    if(cursor().shape() != shape)
        setCursor(shape);

    m_lastMousePos = mousePos;
}