#include "clickinstrument.h"
#include <QGraphicsSceneMouseEvent>
#include <QApplication>

ClickInstrument::ClickInstrument(WatchedType watchedType, QObject *parent): 
    Instrument(
        watchedType,
        makeSet(QEvent::GraphicsSceneMousePress, QEvent::GraphicsSceneMouseMove, QEvent::GraphicsSceneMouseRelease, QEvent::GraphicsSceneMouseDoubleClick),
        parent
    ),
    m_handler(this)
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
    return m_handler.mousePressEvent(event);
}

bool ClickInstrument::mouseMoveEvent(QGraphicsItem *, QGraphicsSceneMouseEvent *event) {
    return m_handler.mouseMoveEvent(event);
}

bool ClickInstrument::mouseReleaseEvent(QGraphicsItem *item, QGraphicsSceneMouseEvent *event) {
    return m_handler.mouseReleaseEvent(item, NULL, event);
}

bool ClickInstrument::mouseDoubleClickEvent(QGraphicsItem *, QGraphicsSceneMouseEvent *event) {
    return m_handler.mouseDoubleClickEvent(event);
}

bool ClickInstrument::mousePressEvent(QGraphicsScene *, QGraphicsSceneMouseEvent *event) {
    return m_handler.mousePressEvent(event);
}

bool ClickInstrument::mouseMoveEvent(QGraphicsScene *, QGraphicsSceneMouseEvent *event) {
    return m_handler.mouseMoveEvent(event);
}

bool ClickInstrument::mouseReleaseEvent(QGraphicsScene *scene, QGraphicsSceneMouseEvent *event) {
    return m_handler.mouseReleaseEvent(NULL, scene, event);
}

bool ClickInstrument::mouseDoubleClickEvent(QGraphicsScene *, QGraphicsSceneMouseEvent *event) {
    return m_handler.mouseDoubleClickEvent(event);
}
