#include "handscrollinstrument.h"
#include <QMouseEvent>
#include <QWidget>
#include <QScrollBar>
#include <QGraphicsView>
#include <QApplication>

HandScrollInstrument::HandScrollInstrument(QObject *parent):
    DragProcessingInstrument(VIEWPORT, makeSet(QEvent::MouseButtonPress, QEvent::MouseMove, QEvent::MouseButtonRelease), parent)
{}

HandScrollInstrument::~HandScrollInstrument() {
    ensureUninstalled();
}

bool HandScrollInstrument::mousePressEvent(QWidget *viewport, QMouseEvent *event) {
    if (event->button() != Qt::RightButton)
        return false;

    processor()->mousePressEvent(viewport, event);

    // TODO: check this /* If we accept the event here we will block right-click. So don't accept it. */
    event->accept();
    return false;
}

bool HandScrollInstrument::mouseMoveEvent(QWidget *viewport, QMouseEvent *event) {
    processor()->mouseMoveEvent(viewport, event);

    event->accept();
    return false; 
}

bool HandScrollInstrument::mouseReleaseEvent(QWidget *viewport, QMouseEvent *event) {
    processor()->mouseReleaseEvent(viewport, event);

    event->accept();
    return false;
}

void HandScrollInstrument::startDrag(QGraphicsView *view) {
    m_originalCursor = view->viewport()->cursor();
    view->viewport()->setCursor(Qt::ClosedHandCursor);

    emit scrollingStarted(view);
}

void HandScrollInstrument::drag(QGraphicsView *view) {
    moveViewport(view, -(processor()->mousePos() - processor()->lastMousePos()));

}

void HandScrollInstrument::finishDrag(QGraphicsView *view) {
    emit scrollingFinished(view);

    view->viewport()->setCursor(m_originalCursor);
}

