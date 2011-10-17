#include "contextmenuinstrument.h"
#include <QAction>
#include <QMenu>
#include <QMouseEvent>
#include <QGraphicsView>
#include <QApplication>

namespace {
    QEvent::Type viewportEventTypes[] = {
        QEvent::MouseButtonPress,
        QEvent::MouseMove,
        QEvent::MouseButtonRelease
    };
}

ContextMenuInstrument::ContextMenuInstrument(QObject *parent) :
    Instrument(makeSet(), makeSet(), makeSet(viewportEventTypes), false, parent),
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

    m_showMenu = true;
    m_mousePressPos = event->pos();

    return false;
}

bool ContextMenuInstrument::mouseMoveEvent(QWidget *viewport, QMouseEvent *event) {
    if (!m_showMenu)
        return false;

    if ((m_mousePressPos - event->pos()).manhattanLength() > QApplication::startDragDistance()) {
        m_showMenu = false;
        return false;
    }

    return false;
}

bool ContextMenuInstrument::mouseReleaseEvent(QWidget *viewport, QMouseEvent *event) {
    if(event->button() != Qt::RightButton)
        return false;

    if(!m_showMenu)
        return false;

    m_contextMenu->popup(event->globalPos());

    return false;
}
