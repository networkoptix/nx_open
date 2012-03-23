#include "hand_scroll_instrument.h"
#include <QMouseEvent>
#include <QWidget>
#include <QScrollBar>
#include <QGraphicsView>
#include <QApplication>
#include <ui/processors/kinetic_cutting_processor.h>
#include <ui/animation/animation_event.h>

HandScrollInstrument::HandScrollInstrument(QObject *parent):
    base_type(VIEWPORT, makeSet(QEvent::MouseButtonPress, QEvent::MouseMove, QEvent::MouseButtonRelease, AnimationEvent::Animation), parent)
{
    KineticCuttingProcessor *processor = new KineticCuttingProcessor(QMetaType::QPointF, this);
    processor->setHandler(this);
    processor->setMaxShiftInterval(0.01);
    processor->setSpeedCuttingThreshold(56); /* In pixels per second. */
    processor->setFriction(384);
    animationTimer()->addListener(processor);
}

HandScrollInstrument::~HandScrollInstrument() {
    ensureUninstalled();
}

void HandScrollInstrument::emulate(QPoint viewportDelta) {
    if(!m_currentView) {
        if(scene()->views().empty()) {
            return;
        } else {
            m_currentView = scene()->views()[0];
        }
    }

    kineticProcessor()->shift(QPointF(viewportDelta));
    kineticProcessor()->start();
}

void HandScrollInstrument::aboutToBeDisabledNotify() {
    base_type::aboutToBeDisabledNotify();

    kineticProcessor()->reset();
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
    if(delta.isNull())
        return;

    kineticProcessor()->shift(QPointF(delta));
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

void HandScrollInstrument::kineticMove(const QVariant &distance) {
    QGraphicsView *view = m_currentView.data();
    if(view == NULL)
        return;

    moveViewport(view, distance.toPointF());
}

void HandScrollInstrument::finishKinetic() {
    m_currentView.clear();
}
