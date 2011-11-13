#include "transformlistenerinstrument.h"

TransformListenerInstrument::TransformListenerInstrument(QObject *parent): 
    Instrument(makeSet(QEvent::Paint), makeSet(), makeSet(), makeSet(), parent)
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

