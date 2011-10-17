#include "wheelzoominstrument.h"
#include <cmath> /* For std::pow. */
#include <QWheelEvent>
#include <QGraphicsView>


WheelZoomInstrument::WheelZoomInstrument(QObject *parent): 
    Instrument(makeSet(), makeSet(), makeSet(QEvent::Wheel), false, parent) 
{}

bool WheelZoomInstrument::wheelEvent(QWidget *viewport, QWheelEvent *event) {
    /* delta() returns the distance that the wheel is rotated 
    * in eighths (1/8s) of a degree. */
    qreal degrees = event->delta() / 8.0;

    /* Use 2x scale for 180 degree turn. */
    qreal scaleFactor = std::pow(2.0, degrees / 180.0);

    /* Scale restoring the transformation anchor. */
    QGraphicsView *view = this->view(viewport);
    QGraphicsView::ViewportAnchor oldAnchor = view->transformationAnchor();
    view->setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    view->scale(scaleFactor, scaleFactor); 
    view->setTransformationAnchor(oldAnchor);

    event->accept();
    return true;
}


