#include "emulated_frame_widget.h"

#include <QtGui/QMouseEvent>

#include <ui/common/frame_section.h>

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
    case QEvent::HoverMove: {
        QHoverEvent *e = static_cast<QHoverEvent *>(event);
        if(childAt(e->pos())) {
            unsetCursor();
        } else {
            Qt::CursorShape cursorShape = Qn::calculateHoverCursorShape(windowFrameSectionAt(e->pos()));
        
            /* There is no equality check in QWidget::setCursor, 
             * and we don't want to trigger unnecessary change events. */
            if(cursor().shape() != cursorShape)
                setCursor(cursorShape);
        }
        event->accept();
        return true;
    }
    case QEvent::HoverLeave:
        unsetCursor();
        event->accept();
        return true;
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

    if(!m_dragProcessor->isWaiting()) {
        
    } else {
    }
}

void QnEmulatedFrameWidget::mouseReleaseEvent(QMouseEvent *event) {
    base_type::mouseReleaseEvent(event);

    m_dragProcessor->widgetMouseReleaseEvent(this, event);
}

void QnEmulatedFrameWidget::dragMove(DragInfo *info) {
    if(m_section == Qt::TitleBarArea) {
        move(pos() + info->mouseScreenPos() - info->lastMouseScreenPos());
    } else {
        resize(m_startSize + Qn::calculateSizeDelta(info->mouseScreenPos() - info->mousePressScreenPos(), m_section));
        move(pos() + m_startPinPoint - Qn::calculatePinPoint(geometry(), m_section));
    }
}

