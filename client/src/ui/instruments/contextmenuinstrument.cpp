#include "contextmenuinstrument.h"
#include <QAction>
#include <QMenu>
#include <QMouseEvent>
#include <QGraphicsView>
#include <QApplication>

ContextMenuInstrument::ContextMenuInstrument(QObject *parent) :
    Instrument(makeSet(QEvent::MouseButtonPress, QEvent::MouseMove, QEvent::MouseButtonRelease), makeSet(), makeSet(), makeSet(), parent),
    m_contextMenu(new QMenu())
{}

ContextMenuInstrument::~ContextMenuInstrument() {}

bool ContextMenuInstrument::mousePressEvent(QWidget *viewport, QMouseEvent *event) {
    if (event->button() != Qt::RightButton)
        return false;

    QGraphicsView *view = this->view(viewport);
    QGraphicsItem *item = this->item(view, event->pos());
    if (item == NULL)
        return false;

    m_isClick = true;
    m_mousePressPos = event->pos();

    return false;
}

bool ContextMenuInstrument::mouseMoveEvent(QWidget *, QMouseEvent *event) {
    if (!m_isClick)
        return false;

    if ((m_mousePressPos - event->pos()).manhattanLength() > QApplication::startDragDistance()) {
        m_isClick = false;
        return false;
    }

    return false;
}

bool ContextMenuInstrument::mouseReleaseEvent(QWidget *, QMouseEvent *event) {
    if(event->button() != Qt::RightButton)
        return false;

    if(!m_isClick)
        return false;

    m_contextMenu->popup(event->globalPos());

    return false;
}
