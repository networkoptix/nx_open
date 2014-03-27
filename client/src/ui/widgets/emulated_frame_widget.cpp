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

    setMouseTracking(true);
}

QnEmulatedFrameWidget::~QnEmulatedFrameWidget() {
    return;
}

void QnEmulatedFrameWidget::timerEvent(QTimerEvent *event) {
    if(event->timerId() != m_timer.timerId()) {
        base_type::timerEvent(event);
    } else {
        updateCursor();
        event->accept();
    }
}

void QnEmulatedFrameWidget::mousePressEvent(QMouseEvent *event) {
    base_type::mousePressEvent(event);

    Qt::WindowFrameSection section = windowFrameSectionAt(event->pos());
    if(section != Qt::NoSection) {
        if (section == Qt::TitleBarArea) {
            m_startPosition = pos();
        } else {
            m_startPinPoint = Qn::calculatePinPoint(geometry(), section);
            m_startSize = size();
        }

        m_section = section;
        m_dragProcessor->widgetMousePressEvent(this, event);
    }
}

void QnEmulatedFrameWidget::mouseMoveEvent(QMouseEvent *event) {
    base_type::mouseMoveEvent(event);

    m_dragProcessor->widgetMouseMoveEvent(this, event);

    if(!event->buttons()) {
        updateCursor(event->pos());
        event->accept();
    }
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
        move(m_startPosition + info->mouseScreenPos() - info->mousePressScreenPos());
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

