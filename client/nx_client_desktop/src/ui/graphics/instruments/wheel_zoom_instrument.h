#ifndef QN_WHEEL_ZOOM_INSTRUMENT_H
#define QN_WHEEL_ZOOM_INSTRUMENT_H

#include "instrument.h"

#include <ui/processors/kinetic_processor.h>
#include <ui/workbench/workbench_context_aware.h>

/**
 * Wheel zoom instrument implements scene zooming with a mouse wheel.
 * 
 * It is to be installed after a scene forwarding instrument.
 */
class WheelZoomInstrument: public Instrument, protected KineticProcessHandler, public QnWorkbenchContextAware {
    Q_OBJECT
public:
    WheelZoomInstrument(QObject *parent);
    virtual ~WheelZoomInstrument();

    void emulate(qreal degrees);

protected:
    virtual void aboutToBeDisabledNotify() override;

    virtual bool wheelEvent(QWidget *viewport, QWheelEvent *event) override;
    virtual bool wheelEvent(QGraphicsScene *scene, QGraphicsSceneWheelEvent *event) override;

    virtual void kineticMove(const QVariant &degrees) override;
    virtual void finishKinetic() override;

private:
    QPointer<QWidget> m_currentViewport;
    QPoint m_viewportAnchor;
};

#endif // QN_WHEEL_ZOOM_INSTRUMENT_H
