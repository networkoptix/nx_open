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
    if(!processor()->isWaiting())
        return false;

    if (event->button() != Qt::RightButton)
        return false;

    processor()->mousePressEvent(viewport, event);

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

void HandScrollInstrument::startDrag() {
    m_originalCursor = processor()->view()->viewport()->cursor();
    processor()->view()->viewport()->setCursor(Qt::ClosedHandCursor);

    emit scrollingStarted(processor()->view());
}

void HandScrollInstrument::drag() {
    moveViewport(processor()->view(), -(processor()->mouseScreenPos() - processor()->lastMouseScreenPos()));
}

void HandScrollInstrument::finishDrag() {
    emit scrollingFinished(processor()->view());

    processor()->view()->viewport()->setCursor(m_originalCursor);
}

