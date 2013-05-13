#include "transform_listener_instrument.h"

TransformListenerInstrument::TransformListenerInstrument(QObject *parent): 
    Instrument(Viewport, makeSet(QEvent::Paint), parent)
{}

bool TransformListenerInstrument::paintEvent(QWidget *viewport, QPaintEvent *) {
    QGraphicsView *view = this->view(viewport);

    QTransform newTransform = view->viewportTransform();
    QTransform &oldTransform = mTransforms[view];
    if(!qFuzzyCompare(newTransform, oldTransform)) {
        oldTransform = newTransform;

        emit transformChanged(view);
    }

    return false;
}

