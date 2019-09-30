#include "debug_info_instrument.h"

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
    #if defined(Q_OS_MACOS) || defined (Q_OS_LINUX)
        const auto command = QString("ps M -p %1 | wc -l").arg(getpid());
        QString output;
        if (auto pipe = popen(command.toStdString().c_str(), "r"))
        {
            std::array<char, 256> buffer;
            while (fgets(buffer.data(), buffer.size(), pipe))
                output += buffer.data();
            pclose(pipe);
        }
        bool isOk = false;
        threadCount = output.trimmed().toInt(&isOk) - 1;
        if (!isOk)
            threadCount.reset();
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
        return debugInfoLines.join("<br>");
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
            NX_INFO(this, lm(QString("Client profiling info: %1")
                .arg(logMessageLines.join(", "))));
        }
        d->logTimer.restart();
    }

    if (changed)
        emit debugInfoChanged(d->debugInfoRichText());

    return false;
}

} // namespace nx::vms::client::desktop
