#include "drag_processor.h"
#include <cassert>
#include <QApplication>
#include <QMouseEvent>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QGraphicsItem>
#include <QGraphicsObject>
#include <utils/common/warnings.h>
#include "drag_process_handler.h"

DragProcessor::DragProcessor(QObject *parent):
    QObject(parent),
    m_handler(NULL),
    m_state(WAITING),
    m_dragTimerId(0),
    m_transitionCounter(0),
    m_firstDragSent(false)
{}

DragProcessor::~DragProcessor() {
    setHandler(NULL);
}

void DragProcessor::reset() {
    transition(static_cast<QMouseEvent *>(NULL), WAITING);
}

void DragProcessor::setHandler(DragProcessHandler *handler) {
    if(handler != NULL && handler->m_processor != NULL) {
        qnWarning("Given handler is already assigned to a processor.");
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

void DragProcessor::checkThread(QObject *object) {
#ifdef _DEBUG
    if(object == NULL) {
        qnNullCritical(object);
        return;
    }

    if(object->thread() != thread()) {
        qnCritical("Processing an event for an object from another thread.");
        return;
    }
#endif
}

void DragProcessor::checkThread(QGraphicsItem *item) {
#ifdef _DEBUG
    if(item == NULL) {
        qnNullCritical(item);
        return;
    }

    QGraphicsObject *object = item->toGraphicsObject();
    if(object != NULL)
        checkThread(static_cast<QObject *>(object));
#endif
}

void DragProcessor::transitionInternalHelper(State newState) {
    /* This code lets us detect nested transitions. */
    m_transitionCounter++;
    int transitionCounter = m_transitionCounter;

    /* Note that handler may trigger another transition. */
    switch(newState) {
    case WAITING:
        switch(m_state) {
        case PREPAIRING:
            killDragTimer();
            m_state = WAITING;
            if(m_handler != NULL)
                m_handler->finishDragProcess(&m_info);
            break;
        case DRAGGING:
            if(m_handler != NULL) {
                m_state = PREPAIRING;
                m_handler->finishDrag(&m_info);
                if(transitionCounter == m_transitionCounter) {
                    /* No nested transitions => go on. */
                    m_state = WAITING;
                    m_handler->finishDragProcess(&m_info);
                }
            } else {
                m_state = WAITING;
            }
            break;
        default:
            return;
        }
        if(transitionCounter == m_transitionCounter) {
            /* No nested transitions => clean everything up. */
            m_viewport = NULL;
            m_object.clear();
            m_info.m_triggerButton = Qt::NoButton;
            m_info.m_view = NULL;
            m_info.m_scene = NULL;
            m_info.m_item = NULL;
        }
        break;
    case PREPAIRING:
        startDragTimer();
        switch(m_state) {
        case WAITING:
            m_state = PREPAIRING;
            if(m_handler != NULL)
                m_handler->startDragProcess(&m_info);
            break;
        case DRAGGING:
            m_state = PREPAIRING;
            if(m_handler != NULL)
                m_handler->finishDrag(&m_info);
            break;
        default:
            return;
        }
        break;
    case DRAGGING:
        switch(m_state) {
        case WAITING:
            if(m_handler != NULL) {
                m_state = PREPAIRING;
                m_handler->startDragProcess(&m_info);
                if(transitionCounter == m_transitionCounter) {
                    /* No nested transitions => go on. */
                    m_state = DRAGGING;
                    m_handler->startDrag(&m_info);
                }
            } else {
                m_state = DRAGGING;
            }
            break;
        case PREPAIRING:
            m_state = DRAGGING;
            killDragTimer();
            if(m_handler != NULL)
                m_handler->startDrag(&m_info);
            break;
        default:
            return;
        }
        if(transitionCounter == m_transitionCounter)
            m_firstDragSent = false;
        break;
    default:
        return;
    }
}

void DragProcessor::transitionInternal(QEvent *event, State newState) {
    m_info.m_event = event;

    transitionInternalHelper(newState);

    m_info.m_event = NULL;
}

void DragProcessor::transition(QEvent *event, State state) {
    if(m_object.isNull()) {
        m_viewport = NULL;
        m_info.m_view = NULL;
        m_info.m_scene = NULL;
        m_info.m_item = NULL;
        transitionInternal(event, WAITING);
        return;
    }

    transitionInternal(event, state);
}

void DragProcessor::transition(QEvent *event, QWidget *widget, State state) {
    m_object = widget;
    m_info.m_widget = widget;
    m_info.m_view = NULL;
    m_info.m_scene = NULL;
    m_info.m_item = NULL;

    transitionInternal(event, state);
}

void DragProcessor::transition(QEvent *event, QGraphicsView *view, State state) {
    m_object = view;
    m_info.m_widget = NULL;
    m_info.m_view = view;
    m_info.m_scene = NULL;
    m_info.m_item = NULL;

    transitionInternal(event, state);
}

void DragProcessor::transition(QEvent *event, QGraphicsScene *scene, State state) {
    m_object = scene;
    m_info.m_widget = NULL;
    m_info.m_view = NULL;
    m_info.m_scene = scene;
    m_info.m_item = NULL;

    transitionInternal(event, state);
}

void DragProcessor::transition(QEvent *event, QGraphicsItem *item, State state) {
    QGraphicsObject *object = item->toGraphicsObject();
    if(object != NULL)
        m_object = object;
    else
        m_object = this; /* Unsafe, but we have no other options. */
    m_info.m_widget = NULL;
    m_info.m_view = NULL;
    m_info.m_scene = NULL;
    m_info.m_item = item;

    transitionInternal(event, state);
}

QPoint DragProcessor::screenPos(QWidget *, QMouseEvent *event) {
    return event->globalPos();
}

template<class T>
QPoint DragProcessor::screenPos(T *, QGraphicsSceneMouseEvent *event) {
    return event->screenPos();
}

QPointF DragProcessor::scenePos(QWidget *, QMouseEvent *) {
    return QPointF(); /* No scene involved. */
}

QPointF DragProcessor::scenePos(QGraphicsView *view, QMouseEvent *event) {
    return view->mapToScene(event->pos());
}

template<class T>
QPointF DragProcessor::scenePos(T *, QGraphicsSceneMouseEvent *event) {
    return event->scenePos();
}

template<class T, class Event>
void DragProcessor::mousePressEventInternal(T *object, Event *event) {
    checkThread(object);

    if(m_state == WAITING) {
        m_info.m_triggerButton = event->button();
        m_info.m_lastMouseScreenPos = m_info.m_mousePressScreenPos = screenPos(object, event);
        m_info.m_lastMouseScenePos = m_info.m_mousePressScenePos = scenePos(object, event);

        transition(event, object, PREPAIRING);
    } else {
        if(m_state == DRAGGING)
            drag(event, screenPos(object, event), scenePos(object, event));

        /* Stop scrolling if the user has let go of the trigger button (even if we didn't get the release event). */
        if(!(event->buttons() & m_info.m_triggerButton))
            transition(event, object, WAITING);
    }
}

void DragProcessor::timerEvent(QTimerEvent *event) {
    transition(static_cast<QMouseEvent *>(NULL), DRAGGING);

    /* Send drag right away. */
    if(m_state == DRAGGING) {
        /* Using press pos here is not 100% correct, but we don't want to 
         * complicate the event handling logic even further to store the
         * current mouse pos. */
        drag(event, m_info.m_mousePressScreenPos, m_info.m_mousePressScenePos); 
    }
}

template<class T, class Event>
void DragProcessor::mouseMoveEventInternal(T *object, Event *event) {
    checkThread(object);

    if (m_state == WAITING)
        return;

    /* Stop dragging if the user has let go of the trigger button (even if we didn't get the release event). */
    if (!(event->buttons() & m_info.m_triggerButton)) {
        transition(event, object, WAITING);
        return;
    }

    /* Check for drag distance. */
    if (m_state == PREPAIRING) {
        if ((m_info.m_mousePressScreenPos - screenPos(object, event)).manhattanLength() < QApplication::startDragDistance()) {
            return;
        } else {
            transition(event, object, DRAGGING);
        }
    }

    /* Perform drag operation. */
    if(m_state == DRAGGING)
        drag(event, screenPos(object, event), scenePos(object, event));
}

void DragProcessor::drag(QEvent *event, const QPoint &screenPos, const QPointF &scenePos) {
    assert(m_state == DRAGGING);

    m_info.m_mouseScreenPos = screenPos;
    m_info.m_mouseScenePos = scenePos;
    m_info.m_event = event;

    if(!m_firstDragSent) {
        m_firstDragSent = true;
    } else if(!(m_flags & DONT_COMPRESS) && m_info.m_mouseScreenPos == m_info.m_lastMouseScreenPos && qFuzzyCompare(m_info.m_mouseScenePos, m_info.m_lastMouseScenePos)) {
        return;
    }

    if(m_handler != NULL)
        m_handler->dragMove(&m_info);

    m_info.m_lastMouseScreenPos = m_info.m_mouseScreenPos;
    m_info.m_lastMouseScenePos = m_info.m_mouseScenePos;
    m_info.m_event = NULL;
}

template<class T, class Event>
void DragProcessor::mouseReleaseEventInternal(T *object, Event *event) {
    checkThread(object);

    if (m_state == WAITING)
        return;

    if(m_state == DRAGGING)
        drag(event, screenPos(object, event), scenePos(object, event));

    if(event->button() == m_info.m_triggerButton) {
        transition(event, object, WAITING);
        return;
    }
}

void DragProcessor::widgetEvent(QWidget *widget, QEvent *event) {
    switch(event->type()) {
    case QEvent::MouseButtonPress:
        widgetMousePressEvent(widget, static_cast<QMouseEvent *>(event));
        break;
    case QEvent::MouseMove:
        widgetMouseMoveEvent(widget, static_cast<QMouseEvent *>(event));
        break;
    case QEvent::MouseButtonRelease:
        widgetMouseReleaseEvent(widget, static_cast<QMouseEvent *>(event));
        break;
    case QEvent::Paint:
        widgetPaintEvent(widget, static_cast<QPaintEvent *>(event));
        break;
    default:
        break;
    }
}

void DragProcessor::widgetMousePressEvent(QWidget *widget, QMouseEvent *event) {
    mousePressEventInternal(widget, event);
}

void DragProcessor::widgetMouseMoveEvent(QWidget *widget, QMouseEvent *event) {
    mouseMoveEventInternal(widget, event);
}

void DragProcessor::widgetMouseReleaseEvent(QWidget *widget, QMouseEvent *event) {
    mouseReleaseEventInternal(widget, event);
}

void DragProcessor::mousePressEvent(QWidget *viewport, QMouseEvent *event) {
    m_viewport = viewport;

    mousePressEventInternal(this->view(viewport), event);
}

void DragProcessor::mouseMoveEvent(QWidget *viewport, QMouseEvent *event) {
    mouseMoveEventInternal(this->view(viewport), event);
}

void DragProcessor::mouseReleaseEvent(QWidget *viewport, QMouseEvent *event) {
    mouseReleaseEventInternal(this->view(viewport), event);
}

void DragProcessor::widgetPaintEvent(QWidget *widget, QPaintEvent *event) {
    checkThread(widget);

    if(m_state == DRAGGING) {
        /* Stop dragging if the user has let go of the trigger button (even if we didn't get the release event). */
        if (!(QApplication::mouseButtons() & m_info.m_triggerButton)) {
            transition(event, WAITING);
            return;
        }

        QPoint screenPos = QCursor::pos();
        drag(event, screenPos, QPointF());
    }
}

void DragProcessor::paintEvent(QWidget *viewport, QPaintEvent *event) {
    checkThread(viewport);

    if(m_state == DRAGGING) {
        /* Stop dragging if the user has let go of the trigger button (even if we didn't get the release event). */
        if (!(QApplication::mouseButtons() & m_info.m_triggerButton)) {
            transition(event, WAITING);
            return;
        }

        if(viewport == m_viewport) {
            QPoint screenPos = QCursor::pos();
            drag(event, screenPos, this->view(viewport)->mapToScene(viewport->mapFromGlobal(screenPos)));
        }
    }
}

void DragProcessor::mousePressEvent(QGraphicsScene *scene, QGraphicsSceneMouseEvent *event) {
    m_viewport = event->widget();

    mousePressEventInternal(scene, event);
}

void DragProcessor::mouseMoveEvent(QGraphicsScene *scene, QGraphicsSceneMouseEvent *event) {
    mouseMoveEventInternal(scene, event);
}

void DragProcessor::mouseReleaseEvent(QGraphicsScene *scene, QGraphicsSceneMouseEvent *event) {
    mouseReleaseEventInternal(scene, event);
}

void DragProcessor::mousePressEvent(QGraphicsItem *item, QGraphicsSceneMouseEvent *event) {
    m_viewport = event->widget();

    mousePressEventInternal(item, event);
}

void DragProcessor::mouseMoveEvent(QGraphicsItem *item, QGraphicsSceneMouseEvent *event) {
    mouseMoveEventInternal(item, event);
}

void DragProcessor::mouseReleaseEvent(QGraphicsItem *item, QGraphicsSceneMouseEvent *event) {
    mouseReleaseEventInternal(item, event);
}



Qt::KeyboardModifiers DragInfo::modifiers() const {
    QEvent::Type type;
    if(m_event == NULL) {
        type = QEvent::None;
    } else {
        type = m_event->type();
    }

    switch(type) {
    case QEvent::GraphicsSceneMouseDoubleClick:
    case QEvent::GraphicsSceneMouseMove:
    case QEvent::GraphicsSceneMousePress:
    case QEvent::GraphicsSceneMouseRelease:
        return static_cast<QGraphicsSceneMouseEvent *>(m_event)->modifiers();
    case QEvent::MouseButtonDblClick:
    case QEvent::MouseButtonPress:
    case QEvent::MouseButtonRelease:
    case QEvent::MouseMove:
        return static_cast<QMouseEvent *>(m_event)->modifiers();
    default:
        return QApplication::keyboardModifiers();
    }
}

QPoint DragInfo::mouseViewportPos() const {
    return view()->viewport()->mapFromGlobal(mouseScreenPos());
}

