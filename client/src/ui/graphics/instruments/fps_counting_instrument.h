#ifndef QN_FPS_COUNTING_INSTRUMENT_H
#define QN_FPS_COUNTING_INSTRUMENT_H

#include "instrument.h"

class FpsCountingInstrument: public Instrument {
    Q_OBJECT;
public:
    FpsCountingInstrument(int updateIntervalMSec, QObject *parent = NULL);

    virtual ~FpsCountingInstrument();

    qreal fps() const;

signals:
    void fpsChanged(qreal fps);

protected:
    virtual void enabledNotify() override;
    virtual void aboutToBeDisabledNotify() override;

    virtual bool paintEvent(QWidget *viewport, QPaintEvent *event) override;

private:
    void updateFps(qreal fps);

private:
    int m_updateInterval;
    qint64 m_lastTime;
    int m_frameCount;
    qreal m_fps;
};

#endif // QN_FPS_COUNTING_INSTRUMENT_H
