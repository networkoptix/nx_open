#include "rotationinstrument.h"
#include <QMouseEvent>

QnRotationInstrument::QnRotationInstrument(QObject *parent):
    Instrument(VIEWPORT, makeSet(QEvent::MouseButtonPress, QEvent::MouseMove, QEvent::MouseButtonRelease), parent)
{}

QnRotationInstrument::~QnRotationInstrument() {
    ensureUninstalled();
}

bool QnRotationInstrument::mousePressEvent(QWidget *viewport, QMouseEvent *event) {
    if(event->button() != Qt::LeftButton)
        return false;

    if(!(event->modifiers() & Qt::AltModifier))
        return false;

    //dragProcessor()

    return false;
}

bool QnRotationInstrument::mouseMoveEvent(QWidget *viewport, QMouseEvent *event) {
    return false;

}

bool QnRotationInstrument::mouseReleaseEvent(QWidget *viewport, QMouseEvent *event) {
    return false;

}

bool QnRotationInstrument::paintEvent(QWidget *viewport, QPaintEvent *event) {
    return false;

}

/*void QnRotationInstrument::startDrag() {

}

void QnRotationInstrument::drag() {

}

void QnRotationInstrument::finishDrag() {

}

*/