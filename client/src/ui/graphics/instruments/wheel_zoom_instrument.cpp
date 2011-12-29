#include "wheel_zoom_instrument.h"
#include <cmath> /* For std::pow. */
#include <QGraphicsSceneWheelEvent>
#include <QGraphicsView>
#include <QCursor>
#include <QDateTime>
#include <ui/processors/kinetic_cutting_processor.h>
#include <ui/animation/animation_event.h>

namespace {
    const qreal degreesFor2x = 180.0;
}

WheelZoomInstrument::WheelZoomInstrument(QObject *parent): 
    Instrument(
        makeSet(QEvent::Wheel, AnimationEvent::Animation), 
        makeSet(),
        makeSet(QEvent::GraphicsSceneWheel),
        makeSet(),
        parent
    )
{
    KineticCuttingProcessor *processor = new KineticCuttingProcessor(QMetaType::QReal, this);
    processor->setHandler(this);
    processor->setMaxShiftInterval(0.4);
    processor->setFriction(degreesFor2x / 2);
    processor->setMaxSpeedMagnitude(degreesFor2x * 8);
    processor->setSpeedCuttingThreshold(degreesFor2x / 3);
    processor->setFlags(KineticProcessor::IGNORE_DELTA_TIME);
    animationTimer()->addListener(processor);
}

void WheelZoomInstrument::aboutToBeDisabledNotify() {
    kineticProcessor()->reset();
}

bool WheelZoomInstrument::wheelEvent(QWidget *viewport, QWheelEvent *) {
    m_currentViewport = viewport;
    return false;
}

bool WheelZoomInstrument::wheelEvent(QGraphicsScene *, QGraphicsSceneWheelEvent *event) {
    QWidget *viewport = m_currentViewport.data();
    if(viewport == NULL)
        return false;

    /* delta() returns the distance that the wheel is rotated 
     * in eighths (1/8s) of a degree. */
    qreal degrees = event->delta() / 8.0;

    m_viewportAnchor = viewport->mapFromGlobal(QCursor::pos());
    kineticProcessor()->shift(degrees);
    kineticProcessor()->start();

    event->accept();
    return false;
}

void WheelZoomInstrument::kineticMove(const QVariant &degrees) {
    QWidget *viewport = m_currentViewport.data();
    if(viewport == NULL)
        return;

    /* 180 degree turn makes a 2x scale. */
    qreal factor = std::pow(2.0, -degrees.toReal() / degreesFor2x);
    scaleViewport(this->view(viewport), factor, m_viewportAnchor);
}

void WheelZoomInstrument::finishKinetic() {
    m_currentViewport.clear();
}

