#include "dragprocessor.h"
#include <cassert>
#include <QApplication>
#include <QMouseEvent>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QGraphicsItem>
#include <QGraphicsObject>
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
    m_modifiers(0),
    m_handler(NULL),
    m_state(WAITING),
    m_dragTimerId(0),
    m_transitionCounter(0)
{}

DragProcessor::~DragProcessor() {
    setHandler(NULL);
}

void DragProcessor::reset() {
    transition(static_cast<QMouseEvent *>(NULL), WAITING);
}

void DragProcessor::setHandler(DragProcessHandler *handler) {
    if(handler != NULL && handler->m_processor != NULL) {
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

void DragProcessor::preprocessEvent(QGraphicsSceneMouseEvent *event) {
    m_modifiers = event->modifiers();
}

void DragProcessor::preprocessEvent(QMouseEvent *event) {
    m_modifiers = event->modifiers();
}

template<class Event>
void DragProcessor::transitionInternal(Event *event, State newState) {
    m_transitionCounter++;
    int transitionCounter = m_transitionCounter;

    if(event != NULL)
        preprocessEvent(event);

    /* Note that handler may trigger another transition. */
    switch(newState) {
    case WAITING:
        switch(m_state) {
        case PREPAIRING:
            killDragTimer();
            m_state = newState;
            if(m_handler != NULL)
                m_handler->finishDragProcess();
            break;
        case DRAGGING:
            if(m_handler != NULL) {
                m_state = PREPAIRING;
                m_handler->finishDrag();
                if(transitionCounter == m_transitionCounter) {
                    m_state = WAITING;
                    m_handler->finishDragProcess();
                }
            } else {
                m_state = newState;
            }
            break;
        default:
            return;
        }
        if(transitionCounter == m_transitionCounter) {
            m_triggerButton = Qt::NoButton;
            m_viewport = NULL;
            m_object.clear();
            m_view = NULL;
            m_scene = NULL;
            m_item = NULL;
        }
        break;
    case PREPAIRING:
        startDragTimer();
        switch(m_state) {
        case WAITING:
            m_state = newState;
            if(m_handler != NULL)
                m_handler->startDragProcess();
            break;
        case DRAGGING:
            m_state = newState;
            if(m_handler != NULL)
                m_handler->finishDrag();
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
                m_handler->startDragProcess();
                if(transitionCounter == m_transitionCounter) {
                    m_state = DRAGGING;
                    m_handler->startDrag();
                }
            } else {
                m_state = newState;
            }
            break;
        case PREPAIRING:
            m_state = newState;
            killDragTimer();
            if(m_handler != NULL)
                m_handler->startDrag();
            break;
        default:
            return;
        }
        break;
    default:
        return;
    }
}

template<class Event>
void DragProcessor::transition(Event *event, State state) {
    if(m_object.isNull()) {
        m_viewport = NULL;
        m_view = NULL;
        m_scene = NULL;
        m_item = NULL;
        transitionInternal(event, WAITING);
        return;
    }

    transitionInternal(event, state);
}

template<class Event>
void DragProcessor::transition(Event *event, QGraphicsView *view, State state) {
    m_object = view;
    m_view = view;
    m_scene = NULL;
    m_item = NULL;

    transitionInternal(event, state);
}

template<class Event>
void DragProcessor::transition(Event *event, QGraphicsScene *scene, State state) {
    m_object = scene;
    m_view = NULL;
    m_scene = scene;
    m_item = NULL;

    transitionInternal(event, state);
}

template<class Event>
void DragProcessor::transition(Event *event, QGraphicsItem *item, State state) {
    QGraphicsObject *object = item->toGraphicsObject();
    if(object != NULL)
        m_object = object;
    else
        m_object = this; /* Unsafe, but we have no other options. */
    m_view = NULL;
    m_scene = NULL;
    m_item = item;

    transitionInternal(event, state);
}

QPoint DragProcessor::screenPos(QGraphicsView *, QMouseEvent *event) {
    return event->globalPos();
}

template<class T>
QPoint DragProcessor::screenPos(T *, QGraphicsSceneMouseEvent *event) {
    return event->screenPos();
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
        m_triggerButton = event->button();
        m_lastMouseScreenPos = m_mousePressScreenPos = screenPos(object, event);
        m_lastMouseScenePos = m_mousePressScenePos = scenePos(object, event);

        transition(event, object, PREPAIRING);
    } else {
        if(m_state == DRAGGING)
            drag(screenPos(object, event), scenePos(object, event));

        /* Stop scrolling if the user has let go of the trigger button (even if we didn't get the release event). */
        if(!(event->buttons() & m_triggerButton))
            transition(event, object, WAITING);
    }
}

void DragProcessor::timerEvent(QTimerEvent *) {
    transition(static_cast<QMouseEvent *>(NULL), DRAGGING);
}

template<class T, class Event>
void DragProcessor::mouseMoveEventInternal(T *object, Event *event) {
    checkThread(object);

    if (m_state == WAITING)
        return;

    /* Stop scrolling if the user has let go of the trigger button (even if we didn't get the release event). */
    if (!(event->buttons() & m_triggerButton)) {
        transition(event, object, WAITING);
        return;
    }

    /* Check for drag distance. */
    if (m_state == PREPAIRING) {
        if ((m_mousePressScreenPos - screenPos(object, event)).manhattanLength() < QApplication::startDragDistance()) {
            return;
        } else {
            transition(event, object, DRAGGING);
        }
    }

    /* Perform drag operation. */
    if(m_state == DRAGGING)
        drag(screenPos(object, event), scenePos(object, event));
}

void DragProcessor::drag(const QPoint &screenPos, const QPointF &scenePos) {
    assert(m_state == DRAGGING);

    m_mouseScreenPos = screenPos;
    m_mouseScenePos = scenePos;

    if(!(m_flags & DONT_COMPRESS))
        if(m_mouseScreenPos == m_lastMouseScreenPos && qFuzzyCompare(m_mouseScenePos, m_lastMouseScenePos))
            return;

    if(m_handler != NULL)
        m_handler->drag();

    m_lastMouseScreenPos = m_mouseScreenPos;
    m_lastMouseScenePos = m_mouseScenePos;
}

template<class T, class Event>
void DragProcessor::mouseReleaseEventInternal(T *object, Event *event) {
    checkThread(object);

    if (m_state == WAITING)
        return;

    if(m_state == DRAGGING)
        drag(screenPos(object, event), scenePos(object, event));

    if(event->button() == m_triggerButton) {
        transition(event, object, WAITING);
        return;
    }
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

void DragProcessor::paintEvent(QWidget *viewport, QPaintEvent *) {
    checkThread(viewport);

    if(m_state == DRAGGING && viewport == m_viewport) {
        QPoint screenPos = QCursor::pos();
        drag(screenPos, this->view(viewport)->mapToScene(viewport->mapFromGlobal(screenPos)));
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

QPoint DragProcessor::mouseViewportPos() const {
    return view()->viewport()->mapFromGlobal(mouseScreenPos());
}