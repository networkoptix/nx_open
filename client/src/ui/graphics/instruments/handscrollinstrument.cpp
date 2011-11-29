#include "handscrollinstrument.h"
#include <QMouseEvent>
#include <QWidget>
#include <QScrollBar>
#include <QGraphicsView>
#include <QApplication>
#include <ui/processors/kineticcuttingprocessor.h>
#include <ui/animation/animation_event.h>

HandScrollInstrument::HandScrollInstrument(QObject *parent):
    DragProcessingInstrument(VIEWPORT, makeSet(QEvent::MouseButtonPress, QEvent::MouseMove, QEvent::MouseButtonRelease, AnimationEvent::Animation), parent)
{
    KineticCuttingProcessor<QPointF> *processor = new KineticCuttingProcessor<QPointF>(this);
    processor->setHandler(this);
    processor->setMaxShiftInterval(0.01);
    processor->setSpeedCuttingThreshold(128); /* In pixels per second. */
    processor->setFriction(128);
    animationTimer()->addListener(processor);
}

HandScrollInstrument::~HandScrollInstrument() {
    ensureUninstalled();
}

bool HandScrollInstrument::mousePressEvent(QWidget *viewport, QMouseEvent *event) {
    if(!dragProcessor()->isWaiting())
        return false;

    if (event->button() != Qt::RightButton)
        return false;

    kineticProcessor()->reset();
    dragProcessor()->mousePressEvent(viewport, event);

    event->accept();
    return false;
}

void HandScrollInstrument::startDragProcess(DragInfo *info) {
    emit scrollProcessStarted(info->view());
}

void HandScrollInstrument::startDrag(DragInfo *info) {
    m_originalCursor = info->view()->viewport()->cursor();
    info->view()->viewport()->setCursor(Qt::ClosedHandCursor);

    emit scrollStarted(info->view());
}

void HandScrollInstrument::dragMove(DragInfo *info) {
    QPoint delta = -(info->mouseScreenPos() - info->lastMouseScreenPos());

    kineticProcessor()->shift(delta);
    moveViewport(info->view(), delta);
}

void HandScrollInstrument::finishDrag(DragInfo *info) {
    emit scrollFinished(info->view());

    info->view()->viewport()->setCursor(m_originalCursor);

    m_currentView = info->view();
    kineticProcessor()->start();
}

void HandScrollInstrument::finishDragProcess(DragInfo *info) {
    emit scrollProcessFinished(info->view());
}

void HandScrollInstrument::kineticMove(const QPointF &distance) {
    QGraphicsView *view = m_currentView.data();
    if(view == NULL)
        return;

    moveViewportF(view, distance);
}

void HandScrollInstrument::finishKinetic() {
    m_currentView.clear();
}
