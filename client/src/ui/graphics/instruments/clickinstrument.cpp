#include "clickinstrument.h"
#include <QGraphicsSceneMouseEvent>
#include <QApplication>

class ClickInfoPrivate: public QGraphicsSceneMouseEvent {};

ClickInstrument::ClickInstrument(Qt::MouseButtons buttons, WatchedType watchedType, QObject *parent): 
    DragProcessingInstrument(
        watchedType,
        makeSet(QEvent::GraphicsSceneMousePress, QEvent::GraphicsSceneMouseMove, QEvent::GraphicsSceneMouseRelease, QEvent::GraphicsSceneMouseDoubleClick),
        parent
    ),
    m_buttons(buttons),
    m_isClick(false),
    m_isDoubleClick(false)
{}

ClickInstrument::~ClickInstrument() {
    ensureUninstalled();
}

template<class T>
bool ClickInstrument::mousePressEventInternal(T *object, QGraphicsSceneMouseEvent *event) {
    if (!(event->button() & m_buttons))
        return false;

    m_isClick = true;
    m_isDoubleClick = false;

    dragProcessor()->mousePressEvent(object, event);
    return false;
}

template<class T>
bool ClickInstrument::mouseDoubleClickEventInternal(T *object, QGraphicsSceneMouseEvent *event) {
    if(!(event->button() & m_buttons))
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
    if(!(event->button() & m_buttons))
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
    ClickInfo info(static_cast<ClickInfoPrivate *>(event));

    if(m_isDoubleClick) {
        emit doubleClicked(view, item, info);
    } else {
        emit clicked(view, item, info);
    }
}

void ClickInstrument::emitSignals(QGraphicsView *view, QGraphicsScene *, QGraphicsSceneMouseEvent *event) {
    ClickInfo info(static_cast<ClickInfoPrivate *>(event));

    if(m_isDoubleClick) {
        emit doubleClicked(view, info);
    } else {
        emit clicked(view, info);
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


Qt::MouseButton ClickInfo::button() const {
    return d->button();
}

Qt::MouseButtons ClickInfo::buttons() const {
    return d->buttons();
}

QPointF ClickInfo::scenePos() const {
    return d->scenePos();
}

QPoint ClickInfo::screenPos() const {
    return d->screenPos();
}

Qt::KeyboardModifiers ClickInfo::modifiers() const {
    return d->modifiers();
}

