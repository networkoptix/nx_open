#pragma once

#include <QtCore/QElapsedTimer>

#include "instrument.h"

class FpsCountingInstrument: public Instrument
{
    Q_OBJECT

public:
    FpsCountingInstrument(int updateIntervalMs, QObject* parent = nullptr);
    virtual ~FpsCountingInstrument() override;

signals:
    void fpsChanged(qreal fps, qint64 frameTimeMs);

protected:
    virtual void enabledNotify() override;
    virtual void aboutToBeDisabledNotify() override;

    virtual bool paintEvent(QWidget* viewport, QPaintEvent* event) override;

private:
    void updateFps(qreal fps);

private:
    const int m_updateIntervalMs;
    QElapsedTimer m_timer;
    int m_frameCount = 0;
};
