#include "clickinstrument.h"
#include <QGraphicsSceneMouseEvent>
#include <QApplication>

ClickInstrument::ClickInstrument(QObject *parent): 
    Instrument(makeSet(), makeSet(), makeSet(), true, parent) 
{}

bool ClickInstrument::mousePressEvent(QGraphicsItem *, QGraphicsSceneMouseEvent *event) {
    if (event->button() != Qt::LeftButton)
        return false;

    m_isClick = true;
    m_isDoubleClick = false;
    m_mousePressPos = event->screenPos();

    return false;
}

bool ClickInstrument::mouseMoveEvent(QGraphicsItem *, QGraphicsSceneMouseEvent *event) {
    if (!m_isClick)
        return false;

    if ((m_mousePressPos - event->screenPos()).manhattanLength() > QApplication::startDragDistance()) {
        m_isClick = false;
        return false;
    }

    return false;
}

bool ClickInstrument::mouseReleaseEvent(QGraphicsItem *item, QGraphicsSceneMouseEvent *event) {
    if(event->button() != Qt::LeftButton)
        return false;

    if(!m_isClick)
        return false;

    QGraphicsView *view = NULL;
    if(event->widget() != NULL)
        view = this->view(event->widget());

    if(m_isDoubleClick) {
        emit doubleClicked(view, item);
    } else {
        emit clicked(view, item);
    }

    return false;
}

bool ClickInstrument::mouseDoubleClickEvent(QGraphicsItem *, QGraphicsSceneMouseEvent *event) {
    if(event->button() != Qt::LeftButton)
        return false;

    m_isClick = true;
    m_isDoubleClick = true;

    return false;
}