#ifndef QN_GRID_ADJUSTMENT_INSTRUMENT_H
#define QN_GRID_ADJUSTMENT_INSTRUMENT_H

#include "instrument.h"

class QnWorkbench;

class GridAdjustmentInstrument: public Instrument {
    Q_OBJECT;
public:
    GridAdjustmentInstrument(QnWorkbench *workbench, QObject *parent = NULL);

    virtual ~GridAdjustmentInstrument();

    qreal speed() const;

    void setSpeed(qreal speed);

    qreal maxSpacing() const;

    void setMaxSpacing(qreal maxSpacing);

protected:
    virtual bool wheelEvent(QWidget *viewport, QWheelEvent *event) override;
    virtual bool wheelEvent(QGraphicsScene *scene, QGraphicsSceneWheelEvent *event) override;

private:
    QnWorkbench *workbench() const;

private:
    QPointer<QWidget> m_currentViewport;
    QPointer<QnWorkbench> m_workbench;
    qreal m_speed;
    qreal m_maxSpacing;
};


#endif // QN_GRID_ADJUSTMENT_INSTRUMENT_H
