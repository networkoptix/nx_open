#include "fps_counting_instrument.h"

#include <nx/utils/math/fuzzy.h>
#include <nx/utils/log/log.h>

FpsCountingInstrument::FpsCountingInstrument(int updateIntervalMs, QObject *parent):
    Instrument(Viewport, makeSet(QEvent::Paint), parent),
    m_updateIntervalMs(updateIntervalMs)
{
    NX_ASSERT(updateIntervalMs > 0);
}

FpsCountingInstrument::~FpsCountingInstrument()
{
    ensureUninstalled();
}

void FpsCountingInstrument::enabledNotify()
{
    m_timer.restart();
    m_frameCount = 0;
}

void FpsCountingInstrument::aboutToBeDisabledNotify()
{
    m_timer.invalidate();
    m_frameCount = 0;
}

bool FpsCountingInstrument::paintEvent(QWidget* /*viewport*/, QPaintEvent* /*event*/)
{
    if (!m_timer.isValid())
        return false;

    ++m_frameCount;

    if (m_timer.hasExpired(m_updateIntervalMs))
    {
        const auto fps = static_cast<qreal>(m_frameCount) * 1000 / (m_timer.elapsed());
        const auto frameTimeMs = m_timer.elapsed() / m_frameCount;
        emit fpsChanged(fps, frameTimeMs);

        m_timer.restart();
        m_frameCount = 0;
    }

    return false;
}
