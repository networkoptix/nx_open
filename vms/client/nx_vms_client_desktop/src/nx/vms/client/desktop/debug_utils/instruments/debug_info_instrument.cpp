#include "debug_info_instrument.h"

#include <QtCore/QElapsedTimer>

#include <client/client_runtime_settings.h>

#include <boost/circular_buffer.hpp>
#include <nx/utils/math/fuzzy.h>
#include <nx/utils/log/log.h>

#include <nx/vms/client/desktop/ini.h>

#if defined(Q_OS_WIN)
    #include "windows.h"
    #include <tlhelp32.h>
#endif

namespace nx::vms::client::desktop {

namespace {

using namespace std::chrono;

static constexpr milliseconds kHandlesInterval = 500ms;
static constexpr milliseconds kThreadsInterval = 1000ms;
static constexpr milliseconds kLogInterval = 1min;

} // namespace

struct DebugInfoInstrument::Private
{
    const bool profilerMode = qnRuntime->isProfilerMode();
    const int updateIntervalMs = profilerMode ? 10000 : 1000;

    QElapsedTimer fpsTimer;
    QElapsedTimer handlesTimer;
    QElapsedTimer threadsTimer;
    QElapsedTimer logTimer;
    QElapsedTimer totalTimer;

    int frameCount = 0;

    QString fps = "----";
    qint64 frameTimeMs = 0;
    boost::circular_buffer<qint64> frameTimePoints{
        static_cast<std::size_t>(ini().storeFrameTimePoints)};
    QString handles;
    QString threads;

    void updateHandles()
    {
        #if defined(Q_OS_WIN)
            const auto processHandle = GetCurrentProcess();
            const auto gdiHandles = GetGuiResources(processHandle, GR_GDIOBJECTS);
            const auto userHandles = GetGuiResources(processHandle, GR_USEROBJECTS);
            handles = QString("\nGDI: %1").arg(gdiHandles);
            handles += QString("\nUser: %1").arg(userHandles);
        #endif
    }

    void updateThreads()
    {
        #if defined(Q_OS_WIN)
            const auto currentProcessId = GetCurrentProcessId();
            const auto snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);

            PROCESSENTRY32 entry = {};
            entry.dwSize = sizeof(entry);

            auto isOk = Process32First(snapshot, &entry);
            while (isOk && entry.th32ProcessID != currentProcessId)
                isOk = Process32Next(snapshot, &entry);

            CloseHandle(snapshot);

            const QString threadCountStr =
                isOk ? QString::number(entry.cntThreads) : QString("N/A");

            threads = QString("\nThreads: %1").arg(threadCountStr);
        #endif
    }

    void updateFps()
    {
        const auto fps = static_cast<qreal>(frameCount) * 1000 / fpsTimer.elapsed();
        frameTimeMs = fpsTimer.elapsed() / frameCount;
        this->fps = QString::number(fps, 'g', 4);
        frameCount = 0;
    }

    QString debugInfoText() const
    {
        return profilerMode
            ? QString("%1 (%2ms)").arg(fps).arg(frameTimeMs) + handles + threads
            : fps;
    }
};

DebugInfoInstrument::DebugInfoInstrument(QObject* parent):
    Instrument(Viewport, makeSet(QEvent::Paint), parent),
    d(new Private())
{
    d->totalTimer.start();
}

DebugInfoInstrument::~DebugInfoInstrument()
{
    ensureUninstalled();
}

std::vector<qint64> DebugInfoInstrument::getFrameTimePoints()
{
    std::vector<qint64> timePoints;
    timePoints.reserve(d->frameTimePoints.size());
    timePoints.insert(timePoints.begin(), d->frameTimePoints.begin(), d->frameTimePoints.end());
    return timePoints;
}

void DebugInfoInstrument::enabledNotify()
{
    d->fpsTimer.restart();
    d->handlesTimer.restart();
    d->threadsTimer.restart();
    d->logTimer.restart();
    d->frameCount = 0;
}

void DebugInfoInstrument::aboutToBeDisabledNotify()
{
    d->logTimer.invalidate();
    d->handlesTimer.invalidate();
    d->threadsTimer.invalidate();
    d->fpsTimer.invalidate();
    d->frameCount = 0;
}

bool DebugInfoInstrument::paintEvent(QWidget* /*viewport*/, QPaintEvent* /*event*/)
{
    if (!d->fpsTimer.isValid())
        return false;

    ++d->frameCount;
    d->frameTimePoints.push_back(d->totalTimer.elapsed());

    bool changed = false;
    if (d->fpsTimer.hasExpired(d->updateIntervalMs))
    {
        d->updateFps();
        d->fpsTimer.restart();
        changed = true;
    }

    if (d->handlesTimer.hasExpired(kHandlesInterval.count()))
    {
        d->updateHandles();
        d->handlesTimer.restart();
        changed = true;
    }

    if (d->threadsTimer.hasExpired(kThreadsInterval.count()))
    {
        d->updateThreads();
        d->threadsTimer.restart();
        changed = true;
    }

    if (d->logTimer.hasExpired(kLogInterval.count()) && !d->handles.isEmpty())
    {
        NX_INFO(this, lm("Client profiling info:") + d->handles);
        d->logTimer.restart();
    }

    if (changed)
        emit debugInfoChanged(d->debugInfoText());

    return false;
}

} // namespace nx::vms::client::desktop
