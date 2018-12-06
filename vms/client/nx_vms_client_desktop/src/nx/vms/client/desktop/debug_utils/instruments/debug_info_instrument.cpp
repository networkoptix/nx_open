#include "debug_info_instrument.h"

#include <QtCore/QElapsedTimer>

#include <client/client_runtime_settings.h>

#include <nx/utils/math/fuzzy.h>
#include <nx/utils/log/log.h>

#if defined(Q_OS_WIN)
    #include "windows.h"
#endif

namespace nx::vms::client::desktop {

struct DebugInfoInstrument::Private
{
    const bool profilerMode = qnRuntime->isProfilerMode() ;
    const int updateIntervalMs = profilerMode ? 10000 : 1000;
    QElapsedTimer timer;
    int frameCount = 0;

    QString debugInfoText() const
    {
        const auto fps = static_cast<qreal>(frameCount) * 1000 / (timer.elapsed());
        const auto frameTimeMs = timer.elapsed() / frameCount;
        QString text = QString::number(fps, 'g', 4);
        if (profilerMode)
        {
            text = QString("%1 (%2ms)").arg(text, frameTimeMs);

            #if defined(Q_OS_WIN)
                const auto processHandle = GetCurrentProcess();
                const auto gdiHandles = GetGuiResources(processHandle, GR_GDIOBJECTS);
                const auto userHandles = GetGuiResources(processHandle, GR_USEROBJECTS);
                text += QString("\nGDI: %1").arg(gdiHandles);
                text += QString("\nUser: %1").arg(userHandles);
            #endif
        }

        return text;
    }
};

DebugInfoInstrument::DebugInfoInstrument(QObject* parent):
    Instrument(Viewport, makeSet(QEvent::Paint), parent),
    d(new Private())
{
}

DebugInfoInstrument::~DebugInfoInstrument()
{
    ensureUninstalled();
}

void DebugInfoInstrument::enabledNotify()
{
    d->timer.restart();
    d->frameCount = 0;
}

void DebugInfoInstrument::aboutToBeDisabledNotify()
{
    d->timer.invalidate();
    d->frameCount = 0;
}

bool DebugInfoInstrument::paintEvent(QWidget* /*viewport*/, QPaintEvent* /*event*/)
{
    if (!d->timer.isValid())
        return false;

    ++d->frameCount;

    if (d->timer.hasExpired(d->updateIntervalMs))
    {
        emit debugInfoChanged(d->debugInfoText());
        d->timer.restart();
        d->frameCount = 0;
    }

    return false;
}

} // namespace nx::vms::client::desktop
