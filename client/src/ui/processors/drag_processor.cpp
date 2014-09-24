#include "drag_processor.h"

#include <cassert>

#include <QtWidgets/QApplication>
#include <QtGui/QMouseEvent>
#include <QtWidgets/QGraphicsSceneMouseEvent>
#include <QtWidgets/QGraphicsScene>
#include <QtWidgets/QGraphicsView>
#include <QtWidgets/QGraphicsItem>
#include <QtWidgets/QGraphicsObject>

#include <utils/common/warnings.h>
#include <utils/common/checked_cast.h>
#include <utils/common/scoped_value_rollback.h>
#include <utils/math/fuzzy.h>

#include "drag_process_handler.h"

DragProcessor::DragProcessor(QObject *parent):
    QObject(parent),
    m_flags(0),
    m_handling(false),
    m_rehandle(false),
    m_startDragDistance(QApplication::startDragDistance()),
    m_startDragTime(QApplication::startDragTime()),
    m_handler(NULL),
    m_state(Waiting),
    m_transitionCounter(0),
    m_dragTimerId(0),
    m_firstDragSent(false)
{}

DragProcessor::~DragProcessor() {
    setHandler(NULL);
}

void DragProcessor::reset() {
    transition(static_cast<QMouseEvent *>(NULL), Waiting);
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

void DragProcessor::setStartDragDistance(int startDragDistance) {
    if(startDragDistance <= 0) {
        qnWarning("Invalid non-positive start drag distance '%1'.", startDragDistance);
        return;
    }

    m_startDragDistance = startDragDistance;
}

void DragProcessor::setStartDragTime(int startDragTime) {
    m_startDragTime = startDragTime;
}

void DragProcessor::startDragTimer() {
    killDragTimer();

    if(m_startDragTime >= 0)
        m_dragTimerId = startTimer(m_startDragTime);
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
#else
    Q_UNUSED(object)
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
#else
    Q_UNUSED(item)
#endif
}

void DragProcessor::transitionInternalHelper(State newState) {
    /* This code lets us detect nested transitions. */
    m_transitionCounter++;
    int transitionCounter = m_transitionCounter;

    /* Note that handler may trigger another transition. */
    switch(newState) {
    case Waiting:
        switch(m_state) {
        case Prepairing:
            killDragTimer();
            m_state = Waiting;
            if(m_handler != NULL)
                m_handler->finishDragProcess(&m_info);
            break;
        case Running:
            if(m_handler != NULL) {
                m_state = Prepairing;
                m_handler->finishDrag(&m_info);
                if(transitionCounter == m_transitionCounter) {
                    /* No nested transitions => go on. */
                    m_state = Waiting;
                    m_handler->finishDragProcess(&m_info);
                }
            } else {
                m_state = Waiting;
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
    case Prepairing:
        startDragTimer();
        switch(m_state) {
        case Waiting:
            m_state = Prepairing;
            if(m_handler != NULL)
                m_handler->startDragProcess(&m_info);
            break;
        case Running:
            m_state = Prepairing;
            if(m_handler != NULL)
                m_handler->finishDrag(&m_info);
            break;
        default:
            return;
        }
        break;
    case Running:
        switch(m_state) {
        case Waiting:
            if(m_handler != NULL) {
                m_state = Prepairing;
                m_handler->startDragProcess(&m_info);
                if(transitionCounter == m_transitionCounter) {
                    /* No nested transitions => go on. */
                    m_state = Running;
                    m_handler->startDrag(&m_info);
                }
            } else {
                m_state = Running;
            }
            break;
        case Prepairing:
            m_state = Running;
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
        transitionInternal(event, Waiting);
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
QPointF DragProcessor::itemPos(T *, Event *) {
    return QPointF(); /* No item involved. */
}

QPointF DragProcessor::itemPos(QGraphicsItem *, QGraphicsSceneMouseEvent *event) {
    return event->pos();
}

template<class T, class Event>
void DragProcessor::mousePressEventInternal(T *object, Event *event, bool instantDrag) {
    checkThread(object);

    if(m_state == Waiting) {
        m_info.m_triggerButton = event->button();
        m_info.m_lastMouseScreenPos = m_info.m_mousePressScreenPos = m_info.m_mouseScreenPos = screenPos(object, event);
        m_info.m_lastMouseScenePos = m_info.m_mousePressScenePos = m_info.m_mouseScenePos = scenePos(object, event);
        m_info.m_lastMouseItemPos = m_info.m_mousePressItemPos = m_info.m_mouseItemPos = itemPos(object, event);

        if(!instantDrag) {
            transition(event, object, Prepairing);
        } else {
            transition(event, object, Running);

            /* Send drag right away if needed. */
            if(m_state == Running)
                drag(event, m_info.m_mousePressScreenPos, m_info.m_mousePressScenePos, m_info.m_mousePressItemPos, false); 
        }
    } else {
        if(m_state == Running)
            drag(event, screenPos(object, event), scenePos(object, event), itemPos(object, event), false);

        /* Stop scrolling if the user has let go of the trigger button (even if we didn't get the release event). */
        if(!(event->buttons() & m_info.m_triggerButton))
            transition(event, object, Waiting);
    }
}

QGraphicsView *DragProcessor::view(QWidget *viewport) {
    assert(viewport != NULL);

    return checked_cast<QGraphicsView *>(viewport->parent());
}

void DragProcessor::timerEvent(QTimerEvent *event) {
    transition(static_cast<QMouseEvent *>(NULL), Running);

    /* Send drag right away. */
    if(m_state == Running) {
        /* Using press pos here is not 100% correct, but we don't want to 
         * complicate the event handling logic even further to store the
         * current mouse pos. */
        drag(event, m_info.m_mousePressScreenPos, m_info.m_mousePressScenePos, m_info.m_mousePressItemPos, false); 
    }
}

template<class T, class Event>
void DragProcessor::mouseMoveEventInternal(T *object, Event *event) {
    checkThread(object);

    if (m_state == Waiting)
        return;

    /* Stop dragging if the user has let go of the trigger button (even if we didn't get the release event). */
    if (!(event->buttons() & m_info.m_triggerButton)) {
        transition(event, object, Waiting);
        return;
    }

    /* Check for drag distance. */
    if (m_state == Prepairing) {
        if ((m_info.m_mousePressScreenPos - screenPos(object, event)).manhattanLength() < m_startDragDistance) {
            return;
        } else {
            transition(event, object, Running);
        }
    }

    /* Perform drag operation. */
    if(m_state == Running)
        drag(event, screenPos(object, event), scenePos(object, event), itemPos(object, event), false);
}

void DragProcessor::redrag() {
    if(m_state != Running) {
        qnWarning("Cannot issue a drag command while not dragging.");
        return;
    }

    drag(NULL, m_info.m_mouseScreenPos, m_info.m_mouseScenePos, m_info.m_mouseItemPos, true);
}

void DragProcessor::drag(QEvent *event, const QPoint &screenPos, const QPointF &scenePos, const QPointF &itemPos, bool alwaysHandle) {
    assert(m_state == Running);

    if(m_handling) {
        m_rehandle = true;
        return;
    }

    m_info.m_mouseScreenPos = screenPos;
    m_info.m_mouseScenePos = scenePos;
    m_info.m_mouseItemPos = itemPos;
    m_info.m_event = event;

    if(!m_firstDragSent) {
        m_firstDragSent = true;
    } else if(!alwaysHandle && !(m_flags & DontCompress) && m_info.m_mouseScreenPos == m_info.m_lastMouseScreenPos && qFuzzyEquals(m_info.m_mouseScenePos, m_info.m_lastMouseScenePos)) {
        return;
    }

    if(m_handler != NULL) {
        QN_SCOPED_VALUE_ROLLBACK(&m_handling, true)
        do {
            m_rehandle = false;
            m_handler->dragMove(&m_info);
        } while(m_rehandle);
    }

    m_info.m_lastMouseScreenPos = m_info.m_mouseScreenPos;
    m_info.m_lastMouseScenePos = m_info.m_mouseScenePos;
    m_info.m_lastMouseItemPos = m_info.m_mouseItemPos;
    m_info.m_event = NULL;
}

template<class T, class Event>
void DragProcessor::mouseReleaseEventInternal(T *object, Event *event) {
    checkThread(object);

    if (m_state == Waiting)
        return;

    if(m_state == Running)
        drag(event, screenPos(object, event), scenePos(object, event), itemPos(object, event), false);

    if(event->button() == m_info.m_triggerButton) {
        transition(event, object, Waiting);
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

void DragProcessor::widgetMousePressEvent(QWidget *widget, QMouseEvent *event, bool instantDrag) {
    mousePressEventInternal(widget, event, instantDrag);
}

void DragProcessor::widgetMouseMoveEvent(QWidget *widget, QMouseEvent *event) {
    mouseMoveEventInternal(widget, event);
}

void DragProcessor::widgetMouseReleaseEvent(QWidget *widget, QMouseEvent *event) {
    mouseReleaseEventInternal(widget, event);
}

void DragProcessor::mousePressEvent(QWidget *viewport, QMouseEvent *event, bool instantDrag) {
    m_viewport = viewport;

    mousePressEventInternal(this->view(viewport), event, instantDrag);
}

void DragProcessor::mouseMoveEvent(QWidget *viewport, QMouseEvent *event) {
    mouseMoveEventInternal(this->view(viewport), event);
}

void DragProcessor::mouseReleaseEvent(QWidget *viewport, QMouseEvent *event) {
    mouseReleaseEventInternal(this->view(viewport), event);
}

void DragProcessor::widgetPaintEvent(QWidget *widget, QPaintEvent *event) {
    checkThread(widget);

    if(m_state == Running) {
        /* Stop dragging if the user has let go of the trigger button (even if we didn't get the release event). */
        if (!(QApplication::mouseButtons() & m_info.m_triggerButton)) {
            transition(event, Waiting);
            return;
        }

        QPoint screenPos = QCursor::pos();
        drag(event, screenPos, QPointF(), QPointF(), false);
    }
}

void DragProcessor::paintEvent(QWidget *viewport, QPaintEvent *event) {
    checkThread(viewport);

    if(m_state == Running) {
        /* Stop dragging if the user has let go of the trigger button (even if we didn't get the release event). */
        if (!(QApplication::mouseButtons() & m_info.m_triggerButton)) {
            transition(event, Waiting);
            return;
        }

        if(viewport == m_viewport) {
            QPoint screenPos = QCursor::pos();
            drag(event, screenPos, this->view(viewport)->mapToScene(viewport->mapFromGlobal(screenPos)), QPointF(), false);
        }
    }
}

void DragProcessor::mousePressEvent(QGraphicsScene *scene, QGraphicsSceneMouseEvent *event, bool instantDrag) {
    m_viewport = event->widget();

    mousePressEventInternal(scene, event, instantDrag);
}

void DragProcessor::mouseMoveEvent(QGraphicsScene *scene, QGraphicsSceneMouseEvent *event) {
    mouseMoveEventInternal(scene, event);
}

void DragProcessor::mouseReleaseEvent(QGraphicsScene *scene, QGraphicsSceneMouseEvent *event) {
    mouseReleaseEventInternal(scene, event);
}

void DragProcessor::mousePressEvent(QGraphicsItem *item, QGraphicsSceneMouseEvent *event, bool instantDrag) {
    m_viewport = event->widget();

    mousePressEventInternal(item, event, instantDrag);
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

