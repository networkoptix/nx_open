#include "clickinstrument.h"
#include <QGraphicsSceneMouseEvent>
#include <QApplication>

ClickInstrument::ClickInstrument(WatchFlags flags, QObject *parent): 
    Instrument(
        (flags & WATCH_SCENE) ? 
            makeSet(QEvent::GraphicsSceneMousePress, QEvent::GraphicsSceneMouseMove, QEvent::GraphicsSceneMouseRelease, QEvent::GraphicsSceneMouseDoubleClick) : 
            makeSet(), 
        makeSet(), 
        makeSet(), 
        (flags & WATCH_ITEM) ? 
            makeSet(QEvent::GraphicsSceneMousePress, QEvent::GraphicsSceneMouseMove, QEvent::GraphicsSceneMouseRelease, QEvent::GraphicsSceneMouseDoubleClick) : 
            makeSet(), 
        parent
    ),
    m_itemHandler(this),
    m_sceneHandler(this)
{}

bool detail::ClickInstrumentHandler::mousePressEvent(QGraphicsSceneMouseEvent *event) {
    if (event->button() != Qt::LeftButton)
        return false;

    m_isClick = true;
    m_isDoubleClick = false;
    m_mousePressPos = event->screenPos();

    return false;
}

bool detail::ClickInstrumentHandler::mouseMoveEvent(QGraphicsSceneMouseEvent *event) {
    if (!m_isClick)
        return false;

    if ((m_mousePressPos - event->screenPos()).manhattanLength() > QApplication::startDragDistance()) {
        m_isClick = false;
        return false;
    }

    return false;
}

bool detail::ClickInstrumentHandler::mouseReleaseEvent(QGraphicsItem *item, QGraphicsScene *scene, QGraphicsSceneMouseEvent *event) {
    if(event->button() != Qt::LeftButton)
        return false;

    if(!m_isClick)
        return false;

    QGraphicsView *view = NULL;
    if(event->widget() != NULL)
        view = m_instrument->view(event->widget());

    if(m_isDoubleClick) {
        if(item != NULL)
            emit m_instrument->doubleClicked(view, item);
        if(scene != NULL)
            emit m_instrument->doubleClicked(view);
    } else {
        if(item != NULL)
            emit m_instrument->clicked(view, item);
        if(scene != NULL)
            emit m_instrument->clicked(view);
    }

    m_isClick = false;
    m_isDoubleClick = false;
    return false;
}

bool detail::ClickInstrumentHandler::mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event) {
    if(event->button() != Qt::LeftButton)
        return false;

    m_isClick = true;
    m_isDoubleClick = true;

    return false;
}

bool ClickInstrument::mousePressEvent(QGraphicsItem *, QGraphicsSceneMouseEvent *event) {
    return m_itemHandler.mousePressEvent(event);
}

bool ClickInstrument::mouseMoveEvent(QGraphicsItem *, QGraphicsSceneMouseEvent *event) {
    return m_itemHandler.mouseMoveEvent(event);
}

bool ClickInstrument::mouseReleaseEvent(QGraphicsItem *item, QGraphicsSceneMouseEvent *event) {
    return m_itemHandler.mouseReleaseEvent(item, NULL, event);
}

bool ClickInstrument::mouseDoubleClickEvent(QGraphicsItem *, QGraphicsSceneMouseEvent *event) {
    return m_itemHandler.mouseDoubleClickEvent(event);
}

bool ClickInstrument::mousePressEvent(QGraphicsScene *, QGraphicsSceneMouseEvent *event) {
    return m_sceneHandler.mousePressEvent(event);
}

bool ClickInstrument::mouseMoveEvent(QGraphicsScene *, QGraphicsSceneMouseEvent *event) {
    return m_sceneHandler.mouseMoveEvent(event);
}

bool ClickInstrument::mouseReleaseEvent(QGraphicsScene *scene, QGraphicsSceneMouseEvent *event) {
    return m_sceneHandler.mouseReleaseEvent(NULL, scene, event);
}

bool ClickInstrument::mouseDoubleClickEvent(QGraphicsScene *, QGraphicsSceneMouseEvent *event) {
    return m_sceneHandler.mouseDoubleClickEvent(event);
}
