#include "fps_counting_instrument.h"
#include <QtCore/QDateTime>
#include <utils/common/warnings.h>

#include <nx/utils/math/fuzzy.h>

FpsCountingInstrument::FpsCountingInstrument(int updateIntervalMSec, QObject *parent):
    Instrument(Viewport, makeSet(QEvent::Paint), parent),
    m_fps(0.0)
{
    if(updateIntervalMSec <= 0) {
        qnWarning("Invalid non-positive fps update interval '%1'.", updateIntervalMSec);
        updateIntervalMSec = 1000; /* Sensible default. */
    }

    m_updateInterval = updateIntervalMSec;
}

FpsCountingInstrument::~FpsCountingInstrument() {
    ensureUninstalled();
}

qreal FpsCountingInstrument::fps() const {
    return m_fps;
}

void FpsCountingInstrument::enabledNotify() {
    m_lastTime = QDateTime::currentMSecsSinceEpoch();
    m_frameCount = 0;
}

void FpsCountingInstrument::aboutToBeDisabledNotify() {
    updateFps(0.0);
}

bool FpsCountingInstrument::paintEvent(QWidget *, QPaintEvent *) {
    m_frameCount++;
    qint64 currentTime = QDateTime::currentMSecsSinceEpoch();

    if(currentTime - m_lastTime > m_updateInterval) {
        updateFps(static_cast<qreal>(m_frameCount) * 1000 / (currentTime - m_lastTime));
        m_lastTime = currentTime;
        m_frameCount = 0;
    }

    return false;
}

void FpsCountingInstrument::updateFps(qreal fps) {
    if (qFuzzyEquals(fps, m_fps))
        return;

    m_fps = fps;
    emit fpsChanged(m_fps);
}

