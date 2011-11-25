#include "rotationinstrument.h"
#include <QMouseEvent>

RotationInstrument::RotationInstrument(QObject *parent):
    DragProcessingInstrument(VIEWPORT, makeSet(QEvent::MouseButtonPress, QEvent::MouseMove, QEvent::MouseButtonRelease), parent)
{}

RotationInstrument::~RotationInstrument() {
    ensureUninstalled();
}

bool RotationInstrument::mousePressEvent(QWidget *viewport, QMouseEvent *event) {
    if(event->button() != Qt::LeftButton)
        return false;

    if(!(event->modifiers() & Qt::AltModifier))
        return false;

    dragProcessor()->mousePressEvent(viewport, event);

    return false;
}

bool RotationInstrument::mouseMoveEvent(QWidget *viewport, QMouseEvent *event) {
    
    return false;
}

bool RotationInstrument::mouseReleaseEvent(QWidget *viewport, QMouseEvent *event) {
    return false;

}

bool RotationInstrument::paintEvent(QWidget *viewport, QPaintEvent *event) {
    return false;

}

void RotationInstrument::startDrag(DragInfo *info) {

}

void RotationInstrument::dragMove(DragInfo *info) {

}

void RotationInstrument::finishDrag(DragInfo *info) {

}

