#include "wheelzoominstrument.h"
#include <cmath> /* For std::pow. */
#include <QWheelEvent>
#include <QGraphicsView>

WheelZoomInstrument::WheelZoomInstrument(QObject *parent): 
    Instrument(VIEWPORT, makeSet(QEvent::Wheel), parent) 
{}

bool WheelZoomInstrument::wheelEvent(QWidget *viewport, QWheelEvent *event) {
    /* delta() returns the distance that the wheel is rotated 
     * in eighths (1/8s) of a degree. */
    qreal degrees = event->delta() / 8.0;

    /* Use 2x scale for 180 degree turn. */
    qreal scaleFactor = std::pow(2.0, -degrees / 180.0);

    /* Scale! */
    scaleViewport(this->view(viewport), scaleFactor, QGraphicsView::AnchorUnderMouse);

    event->accept();
    return true;
}


