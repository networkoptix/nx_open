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
        return;

    //dragProcessor()
}

bool QnRotationInstrument::mouseMoveEvent(QWidget *viewport, QMouseEvent *event) {

}

bool QnRotationInstrument::mouseReleaseEvent(QWidget *viewport, QMouseEvent *event) {

}

bool QnRotationInstrument::paintEvent(QWidget *viewport, QPaintEvent *event) {

}

/*void QnRotationInstrument::startDrag() {

}

void QnRotationInstrument::drag() {

}

void QnRotationInstrument::finishDrag() {

}

*/