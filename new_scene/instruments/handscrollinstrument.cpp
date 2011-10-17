#include "handscrollinstrument.h"
#include <QMouseEvent>
#include <QWidget>
#include <QScrollBar>
#include <QGraphicsView>
#include <QApplication>


namespace {
    QEvent::Type viewportEventTypes[] = {
        QEvent::MouseButtonPress,
        QEvent::MouseMove,
        QEvent::MouseButtonRelease
    };
}

HandScrollInstrument::HandScrollInstrument(QObject *parent):
    Instrument(makeSet(), makeSet(), makeSet(viewportEventTypes), false, parent)
{}

void HandScrollInstrument::installedNotify() {
    m_state = INITIAL;

    Instrument::installedNotify();
}

bool HandScrollInstrument::mousePressEvent(QWidget *, QMouseEvent *event) {
    if (m_state != INITIAL)
        return false;

    if (event->button() != Qt::RightButton)
        return false;

    m_state = PREPAIRING;
    m_mousePressPos = event->pos();
    m_lastMousePos = event->pos();

    /* If we accept the event here we will block right-click. So don't accept it. */
    return false;
}

bool HandScrollInstrument::mouseMoveEvent(QWidget *viewport, QMouseEvent *event) {
    QGraphicsView *view = this->view(viewport);

    if (m_state == INITIAL)
        return false;

    /* Stop scrolling if the user has let go of the trigger button (even if we didn't get the release events). */
    if (!(event->buttons() & Qt::RightButton)) {
        if (m_state == SCROLLING)
            viewport->setCursor(m_originalCursor);
        m_state = INITIAL;
        return false;
    }

    /* Check for drag distance. */
    if (m_state == PREPAIRING) {
        if ((m_mousePressPos - event->pos()).manhattanLength() < QApplication::startDragDistance()) {
            return false;
        } else {
            m_originalCursor = viewport->cursor();
            viewport->setCursor(Qt::ClosedHandCursor);
            m_state = SCROLLING;
        }
    }

    QScrollBar *hBar = view->horizontalScrollBar();
    QScrollBar *vBar = view->verticalScrollBar();
    QPoint delta = event->pos() - m_lastMousePos;
    hBar->setValue(hBar->value() + (view->isRightToLeft() ? delta.x() : -delta.x()));
    vBar->setValue(vBar->value() - delta.y());

    m_lastMousePos = event->pos();

    event->accept();
    return false; /* Let other instruments receive mouse move events too! */
}

bool HandScrollInstrument::mouseReleaseEvent(QWidget *viewport, QMouseEvent *event) {
    if (m_state == INITIAL)
        return false;

    if (event->button() != Qt::RightButton)
        return false;

    if (m_state == SCROLLING)
        viewport->setCursor(m_originalCursor);
    m_state = INITIAL;

    event->accept();
    return true;
}


