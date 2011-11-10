#include "transformlistenerinstrument.h"

TransformListenerInstrument::TransformListenerInstrument(QObject *parent): 
    Instrument(makeSet(), makeSet(), makeSet(QEvent::Paint), makeSet(), parent)
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

