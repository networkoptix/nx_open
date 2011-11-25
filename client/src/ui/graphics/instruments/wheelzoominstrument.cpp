#include "wheelzoominstrument.h"
#include <cmath> /* For std::pow. */
#include <QGraphicsSceneWheelEvent>
#include <QGraphicsView>

WheelZoomInstrument::WheelZoomInstrument(QObject *parent): 
    Instrument(
        makeSet(QEvent::Wheel), 
        makeSet(),
        makeSet(QEvent::GraphicsSceneWheel),
        makeSet(),
        parent
    ) 
{}

bool WheelZoomInstrument::wheelEvent(QWidget *viewport, QWheelEvent *) {
    m_currentViewport = viewport;
    return false;
}

bool WheelZoomInstrument::wheelEvent(QGraphicsScene *scene, QGraphicsSceneWheelEvent *event) {
    QWidget *viewport = m_currentViewport.data();
    if(viewport == NULL)
        return false;

    /* delta() returns the distance that the wheel is rotated 
     * in eighths (1/8s) of a degree. */
    qreal degrees = event->delta() / 8.0;

    /* Use 2x scale for 180 degree turn. */
    qreal scaleFactor = std::pow(2.0, -degrees / 180.0);

    /* Scale! */
    scaleViewport(this->view(viewport), scaleFactor, QGraphicsView::AnchorUnderMouse);

    event->accept();
    m_currentViewport.clear();
    return false;
}
