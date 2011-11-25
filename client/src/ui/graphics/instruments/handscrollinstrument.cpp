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
    if(!dragProcessor()->isWaiting())
        return false;

    if (event->button() != Qt::RightButton)
        return false;

    dragProcessor()->mousePressEvent(viewport, event);

    event->accept();
    return false;
}

void HandScrollInstrument::startDrag(DragInfo *info) {
    m_originalCursor = info->view()->viewport()->cursor();
    info->view()->viewport()->setCursor(Qt::ClosedHandCursor);

    emit scrollingStarted(info->view());
}

void HandScrollInstrument::dragMove(DragInfo *info) {
    moveViewport(info->view(), -(info->mouseScreenPos() - info->lastMouseScreenPos()));
}

void HandScrollInstrument::finishDrag(DragInfo *info) {
    emit scrollingFinished(info->view());

    info->view()->viewport()->setCursor(m_originalCursor);
}

