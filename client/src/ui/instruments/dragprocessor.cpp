#include "dragprocessor.h"
#include <QApplication>
#include <utils/common/warnings.h>

DragProcessHandler::DragProcessHandler():
    m_processor(NULL) 
{}

DragProcessHandler::~DragProcessHandler() {
    if(m_processor != NULL)
        m_processor->setHandler(NULL);
}

DragProcessor::DragProcessor(QObject *parent):
    QObject(parent),
    m_handler(NULL),
    m_state(INITIAL),
    m_dragTimerId(0)
{}

DragProcessor::~DragProcessor() {
    setHandler(NULL);
}

void DragProcessor::reset() {
    transition(INITIAL);
}

void DragProcessor::startDragTimer() {
    killDragTimer();

    m_dragTimerId = startTimer(QApplication::startDragTime());
}

void DragProcessor::killDragTimer() {
    if(m_dragTimerId == 0)
        return;

    killTimer(m_dragTimerId);
    m_dragTimerId = 0;
}

void DragProcessor::checkThread(QWidget *viewport) {
#ifdef _DEBUG
    if(viewport == NULL) {
        qnNullCritical(viewport);
        return;
    }

    if(viewport->thread() != thread()) {
        qnCritical("Processing an event for a viewport from another thread.");
        return;
    }
#endif
}

void DragProcessor::transition(QGraphicsView *view, State state) {
    switch(state) {
    case INITIAL:
        switch(m_state) {
        case PREPAIRING:
            if(m_handler != NULL)
                m_handler->finishDragProcess(view);
            killDragTimer();
            break;
        case DRAGGING:
            if(m_handler != NULL) {
                m_handler->finishDrag(view);
                m_handler->finishDragProcess(view);
            }
            break;
        default:
            return;
        }
        m_triggerButton = Qt::NoButton;
        m_view.clear();
        break;
    case PREPAIRING:
        switch(m_state) {
        case INITIAL:
            if(m_handler != NULL)
                m_handler->startDragProcess(view);
            break;
        case DRAGGING:
            if(m_handler != NULL)
                m_handler->finishDrag(view);
            break;
        default:
            return;
        }
        startDragTimer();
        m_view = view;
        break;
    case DRAGGING:
        switch(m_state) {
        case INITIAL:
            if(m_handler != NULL) {
                m_handler->startDragProcess(view);
                m_handler->startDrag(view);
            }
            break;
        case PREPAIRING:
            if(m_handler != NULL)
                m_handler->startDrag(view);
            killDragTimer();
            break;
        default:
            return;
        }
        m_view = view;
        break;
    default:
        return;
    }

    m_state = state;
}

void DragProcessor::transition(State state) {
    QGraphicsView *view = m_view.data();
    if(view == NULL) {
        transition(view, INITIAL);
        return;
    }

    transition(view, state);
}

void DragProcessor::drag(QGraphicsView *view, const QPoint &pos) {
    m_mousePos = pos;
    m_mouseScenePos = view->mapToScene(pos);

    if(!(m_flags & DONT_COMPRESS))
        if(m_mousePos == m_lastMousePos && qFuzzyCompare(m_mouseScenePos, m_lastMouseScenePos))
            return;

    if(m_handler != NULL)
        m_handler->drag(view);

    m_lastMousePos = m_mousePos;
    m_lastMouseScenePos = m_mouseScenePos;
}

void DragProcessor::setHandler(DragProcessHandler *handler) {
    if(handler->m_processor != NULL) {
        qnWarning("Given listener is already assigned to a processor.");
        return;
    }

    if(m_handler != NULL) {
        reset();
        m_handler->m_processor = NULL;
    }

    m_handler = handler;

    if(m_handler != NULL)
        m_handler->m_processor = this;
}

void DragProcessor::mousePressEvent(QWidget *viewport, QMouseEvent *event) {
    checkThread(viewport);

    QGraphicsView *view = this->view(viewport);

    if(m_state == INITIAL) {
        m_triggerButton = event->button();
        m_lastMousePos = m_mousePressPos = event->pos();
        m_lastMouseScenePos = m_mousePressScenePos = view->mapToScene(event->pos());

        transition(view, PREPAIRING);
    } else {
        drag(view, event->pos());

        /* Stop scrolling if the user has let go of the trigger button (even if we didn't get the release event). */
        if(!(event->buttons() & m_triggerButton))
            transition(view, INITIAL);
    }
}

void DragProcessor::timerEvent(QTimerEvent *) {
    transition(DRAGGING);
}

void DragProcessor::mouseMoveEvent(QWidget *viewport, QMouseEvent *event) {
    checkThread(viewport);

    if (m_state == INITIAL)
        return;

    QGraphicsView *view = this->view(viewport);

    /* Stop scrolling if the user has let go of the trigger button (even if we didn't get the release event). */
    if (!(event->buttons() & m_triggerButton)) {
        transition(view, INITIAL);
        return;
    }

    /* Check for drag distance. */
    if (m_state == PREPAIRING) {
        if ((m_mousePressPos - event->pos()).manhattanLength() < QApplication::startDragDistance()) {
            return;
        } else {
            transition(view, DRAGGING);
        }
    }

    /* Perform drag operation. */
    drag(view, event->pos());
}

void DragProcessor::mouseReleaseEvent(QWidget *viewport, QMouseEvent *event) {
    checkThread(viewport);

    if (m_state == INITIAL)
        return;

    QGraphicsView *view = this->view(viewport);

    drag(view, event->pos());

    if(event->button() == m_triggerButton) {
        transition(view, INITIAL);
        return;
    }
}

void DragProcessor::paintEvent(QWidget *viewport, QPaintEvent *) {
    checkThread(viewport);

    drag(this->view(viewport), viewport->mapFromGlobal(QCursor::pos()));
}

