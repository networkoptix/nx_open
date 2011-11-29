#include "wheelzoominstrument.h"
#include <cmath> /* For std::pow. */
#include <QGraphicsSceneWheelEvent>
#include <QGraphicsView>
#include <ui/processors/kineticzoomprocessor.h>

namespace {
    const qreal degreesFor2x = 180.0;
}

WheelZoomInstrument::WheelZoomInstrument(QObject *parent): 
    Instrument(
        makeSet(QEvent::Wheel), 
        makeSet(),
        makeSet(QEvent::GraphicsSceneWheel),
        makeSet(),
        parent
    ) 
{
    KineticZoomProcessor *processor = new KineticZoomProcessor(this);
    processor->setHandler(this);
    processor->setMaxShiftInterval(0.5);
    processor->setFriction(degreesFor2x / 2);
    processor->setMaxSpeedMagnitude(degreesFor2x * 8);
    processor->setSpeedCuttingThreshold(degreesFor2x / 4);
}

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

    kineticProcessor()->shift(degrees);
    kineticProcessor()->start();

    /* Use 2x scale for 180 degree turn. */
    //qreal scaleFactor = std::pow(2.0, -degrees / 180.0);

    /* Scale! */
    //scaleViewport(this->view(viewport), scaleFactor, QGraphicsView::AnchorUnderMouse);

    event->accept();
    //m_currentViewport.clear();
    return false;
}

void WheelZoomInstrument::kineticMove(const qreal &degrees) {
    QWidget *viewport = m_currentViewport.data();
    if(viewport == NULL)
        return;

    /* 180 degree turn makes a 2x scale. */
    qreal factor = std::pow(2.0, -degrees / degreesFor2x);
    scaleViewport(this->view(viewport), factor, QGraphicsView::AnchorUnderMouse);
}

void WheelZoomInstrument::finishKinetic() {
    m_currentViewport.clear();
}

