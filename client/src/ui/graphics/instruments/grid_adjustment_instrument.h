#ifndef QN_GRID_ADJUSTMENT_INSTRUMENT_H
#define QN_GRID_ADJUSTMENT_INSTRUMENT_H

#include "instrument.h"

class QnWorkbench;

class GridAdjustmentInstrument: public Instrument {
    Q_OBJECT;
public:
    GridAdjustmentInstrument(QnWorkbench *workbench, QObject *parent = NULL);

    virtual ~GridAdjustmentInstrument();

    const QSizeF &speed() const {
        return m_speed;
    }

    void setSpeed(const QSizeF &speed) {
        m_speed = speed;
    }

    const QSizeF &maxSpacing() const {
        return m_maxSpacing;
    }

    void setMaxSpacing(const QSizeF &maxSpacing) {
        m_maxSpacing = maxSpacing;
    }

protected:
    virtual bool wheelEvent(QWidget *viewport, QWheelEvent *event) override;
    virtual bool wheelEvent(QGraphicsScene *scene, QGraphicsSceneWheelEvent *event) override;

private:
    QnWorkbench *workbench() const {
        return m_workbench.data();
    }

private:
    QWeakPointer<QWidget> m_currentViewport;
    QWeakPointer<QnWorkbench> m_workbench;
    QSizeF m_speed;
    QSizeF m_maxSpacing;
};


#endif // QN_GRID_ADJUSTMENT_INSTRUMENT_H
