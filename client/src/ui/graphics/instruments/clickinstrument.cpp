#include "clickinstrument.h"
#include <QGraphicsSceneMouseEvent>
#include <QApplication>

ClickInstrument::ClickInstrument(Qt::MouseButton button, WatchedType watchedType, QObject *parent): 
    DragProcessingInstrument(
        watchedType,
        makeSet(QEvent::GraphicsSceneMousePress, QEvent::GraphicsSceneMouseMove, QEvent::GraphicsSceneMouseRelease, QEvent::GraphicsSceneMouseDoubleClick),
        parent
    ),
    m_button(button),
    m_isClick(false),
    m_isDoubleClick(false)
{}

ClickInstrument::~ClickInstrument() {
    ensureUninstalled();
}

template<class T>
bool ClickInstrument::mousePressEventInternal(T *object, QGraphicsSceneMouseEvent *event) {
    if (event->button() != m_button)
        return false;

    m_isClick = true;
    m_isDoubleClick = false;

    dragProcessor()->mousePressEvent(object, event);
    return false;
}

template<class T>
bool ClickInstrument::mouseDoubleClickEventInternal(T *object, QGraphicsSceneMouseEvent *event) {
    if(event->button() != m_button)
        return false;

    m_isClick = true;
    m_isDoubleClick = true;

    dragProcessor()->mousePressEvent(object, event);
    return false;
}

template<class T>
bool ClickInstrument::mouseMoveEventInternal(T *object, QGraphicsSceneMouseEvent *event) {
    if (!m_isClick)
        return false;

    dragProcessor()->mouseMoveEvent(object, event);
    return false;
}

template<class T>
bool ClickInstrument::mouseReleaseEventInternal(T *object, QGraphicsSceneMouseEvent *event) {
    if(event->button() != m_button)
        return false;

    if(!m_isClick)
        return false;
    
    /* Note that qobject_cast is for a reason here. There are no guarantees
     * regarding the widget stored in the supplied event. */
    QGraphicsView *view = NULL;
    if(event->widget() != NULL)
        view = qobject_cast<QGraphicsView *>(event->widget()->parent()); 
    emitSignals(view, object, event);

    m_isClick = false;
    m_isDoubleClick = false;

    dragProcessor()->mouseReleaseEvent(object, event);
    return false;
}

void ClickInstrument::emitSignals(QGraphicsView *view, QGraphicsItem *item, QGraphicsSceneMouseEvent *event) {
    if(m_isDoubleClick) {
        emit doubleClicked(view, item, event->screenPos());
    } else {
        emit clicked(view, item, event->screenPos());
    }
}

void ClickInstrument::emitSignals(QGraphicsView *view, QGraphicsScene *, QGraphicsSceneMouseEvent *event) {
    if(m_isDoubleClick) {
        emit doubleClicked(view, event->screenPos());
    } else {
        emit clicked(view, event->screenPos());
    }
}

void ClickInstrument::startDrag(DragInfo *) {
    dragProcessor()->reset();
}

void ClickInstrument::finishDragProcess(DragInfo *) {
    m_isClick = false;
    m_isDoubleClick = false;
}

bool ClickInstrument::mousePressEvent(QGraphicsItem *item, QGraphicsSceneMouseEvent *event) {
    return mousePressEventInternal(item, event);
}

bool ClickInstrument::mouseDoubleClickEvent(QGraphicsItem *item, QGraphicsSceneMouseEvent *event) {
    return mouseDoubleClickEventInternal(item, event);
}

bool ClickInstrument::mouseMoveEvent(QGraphicsItem *item, QGraphicsSceneMouseEvent *event) {
    return mouseMoveEventInternal(item, event);
}

bool ClickInstrument::mouseReleaseEvent(QGraphicsItem *item, QGraphicsSceneMouseEvent *event) {
    return mouseReleaseEventInternal(item, event);
}

bool ClickInstrument::mousePressEvent(QGraphicsScene *scene, QGraphicsSceneMouseEvent *event) {
    return mousePressEventInternal(scene, event);
}

bool ClickInstrument::mouseDoubleClickEvent(QGraphicsScene *scene, QGraphicsSceneMouseEvent *event) {
    return mouseDoubleClickEventInternal(scene, event);
}

bool ClickInstrument::mouseMoveEvent(QGraphicsScene *scene, QGraphicsSceneMouseEvent *event) {
    return mouseMoveEventInternal(scene, event);
}

bool ClickInstrument::mouseReleaseEvent(QGraphicsScene *scene, QGraphicsSceneMouseEvent *event) {
    return mouseReleaseEventInternal(scene, event);
}

