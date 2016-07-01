#include "transform_listener_instrument.h"

TransformListenerInstrument::TransformListenerInstrument(QObject *parent): 
    Instrument(Viewport, makeSet(QEvent::Paint, QEvent::Resize), parent)
{}

bool TransformListenerInstrument::paintEvent(QWidget *viewport, QPaintEvent *) {
    QGraphicsView *view = this->view(viewport);

    QTransform newTransform = view->viewportTransform();
    QTransform &oldTransform = m_transforms[view];
    if(!qFuzzyCompare(newTransform, oldTransform)) {
        oldTransform = newTransform;

        emit transformChanged(view);
    }

    return false;
}

bool TransformListenerInstrument::resizeEvent(QWidget *viewport, QResizeEvent *) {
    emit sizeChanged(this->view(viewport));

    return false;
}
