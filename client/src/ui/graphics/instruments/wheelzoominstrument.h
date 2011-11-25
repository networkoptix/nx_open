#ifndef QN_WHEEL_ZOOM_INSTRUMENT_H
#define QN_WHEEL_ZOOM_INSTRUMENT_H

#include "instrument.h"
#include <QWeakPointer>

/**
 * Wheel zoom instrument implements scene zooming with a mouse wheel.
 * 
 * It is to be installed after a scene forwarding instrument.
 */
class WheelZoomInstrument: public Instrument {
    Q_OBJECT;
public:
    WheelZoomInstrument(QObject *parent);

protected:
    virtual bool wheelEvent(QWidget *viewport, QWheelEvent *event) override;
    virtual bool wheelEvent(QGraphicsScene *scene, QGraphicsSceneWheelEvent *event) override;

private:
    QWeakPointer<QWidget> m_currentViewport;
};

#endif // QN_WHEEL_ZOOM_INSTRUMENT_H
