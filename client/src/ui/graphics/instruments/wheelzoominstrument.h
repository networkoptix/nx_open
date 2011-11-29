#ifndef QN_WHEEL_ZOOM_INSTRUMENT_H
#define QN_WHEEL_ZOOM_INSTRUMENT_H

#include "instrument.h"
#include <QWeakPointer>
#include <ui/processors/kineticprocessor.h>

/**
 * Wheel zoom instrument implements scene zooming with a mouse wheel.
 * 
 * It is to be installed after a scene forwarding instrument.
 */
class WheelZoomInstrument: public Instrument, protected KineticProcessHandler<qreal> {
    Q_OBJECT;
public:
    WheelZoomInstrument(QObject *parent);

protected:
    virtual bool wheelEvent(QWidget *viewport, QWheelEvent *event) override;
    virtual bool wheelEvent(QGraphicsScene *scene, QGraphicsSceneWheelEvent *event) override;

    virtual void kineticMove(const qreal &distance) override;
    virtual void finishKinetic() override;

private:
    QWeakPointer<QWidget> m_currentViewport;
    QPoint m_viewportAnchor;
};

#endif // QN_WHEEL_ZOOM_INSTRUMENT_H
