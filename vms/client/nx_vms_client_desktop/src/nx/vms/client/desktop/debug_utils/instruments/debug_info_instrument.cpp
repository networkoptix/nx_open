// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "debug_info_instrument.h"

#include <QtCore/QTimer>
#include <QtCore/QJsonObject>

#include <boost/circular_buffer.hpp>

#include <client/client_runtime_settings.h>
#include <nx/utils/log/log.h>
#include <nx/utils/math/fuzzy.h>
#include <nx/utils/trace/trace.h>
#include <nx/vms/client/desktop/ini.h>
#include <nx/vms/common/html/html.h>

#if defined(Q_OS_WIN)
    #include "windows.h"
    #include <tlhelp32.h>
#endif
#if defined(Q_OS_MACOS)
    #include <mach/mach.h>
    #include <mach/mach_vm.h>
#endif
#if defined(Q_OS_LINUX)
    #include <QtCore/QDir>
#endif

namespace nx::vms::client::desktop {

namespace {

using namespace std::chrono;

#if defined(Q_OS_WIN)
    static constexpr milliseconds kHandlesInterval = 500ms;
#endif
static constexpr milliseconds kThreadsInterval = 1000ms;
static constexpr milliseconds kLogInterval = 1min;
static constexpr auto kCanaryInterval = 1s;

static constexpr int kMaxThreadCount = 250; //< More is considered as suspicious.

QString highlightedLine(const QString& line)
{
    return QString("<span style=\"font-weight: bold; color: red;\">%1</span>").arg(line);
}

} // namespace

DebugMetric::DebugMetric(std::chrono::milliseconds interval, bool profileMode)
    :updateInterval(interval), profileMode(profileMode)
{}

void DebugMetric::restart()
{
    timer.restart();
}

void DebugMetric::invalidate()
{
    timer.invalidate();
}

bool DebugMetric::needProfileMode() const
{
    return profileMode;
}

bool DebugMetric::check()
{
    if (!timer.hasExpired(updateInterval.count()))
        return false;
    updateData();
    timer.restart();
    return true;
}

#if defined(Q_OS_WIN)
class WindowsHandlesMetric: public DebugMetric
{
public:
    WindowsHandlesMetric(): DebugMetric(kHandlesInterval) {}

    virtual void submitData(QStringList& debugInfoLines, bool /*toLog*/) override
    {
        if (gdiHandleCount)
            debugInfoLines.append(QString("GDI: %1").arg(*gdiHandleCount));
        if (userHandleCount)
            debugInfoLines.append(QString("User: %1").arg(*userHandleCount));
    }

    virtual void updateData() override
    {
        const auto processHandle = GetCurrentProcess();
        gdiHandleCount = GetGuiResources(processHandle, GR_GDIOBJECTS);
        userHandleCount = GetGuiResources(processHandle, GR_USEROBJECTS);
    }

    std::optional<int> gdiHandleCount;
    std::optional<int> userHandleCount;
};
#endif

class ThreadsMetric: public DebugMetric
{
public:
    ThreadsMetric(): DebugMetric(kThreadsInterval) {}

    virtual void submitData(QStringList& debugInfoLines, bool toLog) override
    {
        if (!threadCount)
            return;
        auto threadsLine = QString("Threads: %1").arg(*threadCount);
        if (*threadCount > kMaxThreadCount && !toLog)
            threadsLine = highlightedLine(threadsLine);
        debugInfoLines.append(threadsLine);
    }

    virtual void updateData() override
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
            if (isOk)
                threadCount = entry.cntThreads;
        #endif
        #if defined(Q_OS_MACOS)
            mach_msg_type_number_t count;
            thread_act_array_t list;
            if (task_threads(mach_task_self(), &list, &count) != KERN_SUCCESS)
                return;
            mach_vm_deallocate(mach_task_self(), (mach_vm_address_t)list, count * sizeof(*list));
            threadCount = count;
        #endif
        #if defined (Q_OS_LINUX)
            const QString path("/proc/self/task");
            threadCount = QDir(path).entryList(QDir::Dirs | QDir::NoDotAndDotDot).count();
        #endif
    }

    std::optional<int> threadCount;
};

struct DebugInfoInstrument::Private
{
    const bool profilerMode = ini().profilerMode;
    const int updateIntervalMs = profilerMode ? 10000 : 1000;

    QElapsedTimer fpsTimer;
    QElapsedTimer logTimer;
    QElapsedTimer totalTimer;
    QElapsedTimer timerDelay;

    int frameCount = 0;

    QString fps = "----";
    qint64 frameTimeMs = 0;
    boost::circular_buffer<qint64> frameTimePoints{
        static_cast<std::size_t>(ini().storeFrameTimePoints)};

    std::list<std::unique_ptr<DebugMetric>> metrics;

    void updateFps()
    {
        const auto fps = static_cast<qreal>(frameCount) * 1000 / fpsTimer.elapsed();
        frameTimeMs = fpsTimer.elapsed() / frameCount;
        this->fps = QString::number(fps, 'g', 4);
        frameCount = 0;
    }

    QString debugInfoRichText() const
    {
        using namespace nx::vms::common;

        QStringList debugInfoLines;
        if (!profilerMode)
        {
            debugInfoLines.append(fps);
        }
        else
        {
            debugInfoLines.append(QString("%1 (%2ms)").arg(fps).arg(frameTimeMs));
            for (auto& metric: metrics)
                metric->submitData(debugInfoLines, /*toLog=*/false);
        }
        return html::document(debugInfoLines.join(html::kLineBreak));
    }
};

DebugInfoInstrument::DebugInfoInstrument(QObject* parent):
    Instrument(Viewport, makeSet(QEvent::Paint), parent),
    d(new Private())
{
    d->totalTimer.start();

    #if defined(Q_OS_WIN)
    addDebugMetric(std::make_unique<WindowsHandlesMetric>());
    #endif
    addDebugMetric(std::make_unique<ThreadsMetric>());

    // Trace counter for timer delay. High delay means the UI is lagging.
    auto timer = new QTimer(this);
    timer->connect(timer, &QTimer::timeout, this,
        [this, timer](){
            NX_TRACE_COUNTER("Timer").args({
                {"delay",
                d->timerDelay.elapsed() - duration_cast<milliseconds>(kCanaryInterval).count()}
            });
            d->timerDelay.restart();
            timer->start();
        });
    // Start timer for the first time.
    timer->setSingleShot(true);
    timer->setInterval(kCanaryInterval);
    d->timerDelay.start();
    timer->start();
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
    d->logTimer.restart();
    d->frameCount = 0;
    for (auto& metric: d->metrics)
        metric->restart();
}

void DebugInfoInstrument::aboutToBeDisabledNotify()
{
    d->logTimer.invalidate();
    d->fpsTimer.invalidate();
    d->frameCount = 0;
    for (auto& metric: d->metrics)
        metric->invalidate();
}

void DebugInfoInstrument::addDebugMetric(std::unique_ptr<DebugMetric> metric)
{
    d->metrics.push_back(std::move(metric));
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

    for (auto& metric: d->metrics)
    {
        if (metric->needProfileMode() && !d->profilerMode)
            continue;
        changed |= metric->check();
    }

    if (d->logTimer.hasExpired(kLogInterval.count()))
    {
        QStringList logMessageLines;

        for (auto& metric: d->metrics)
            metric->submitData(logMessageLines, /*toLog=*/true);

        if (!logMessageLines.empty())
        {
            NX_INFO(this, nx::format(QString("Client profiling info: %1")
                .arg(logMessageLines.join(", "))));
        }
        d->logTimer.restart();
    }

    if (changed)
        emit debugInfoChanged(d->debugInfoRichText());

    return false;
}

} // namespace nx::vms::client::desktop
