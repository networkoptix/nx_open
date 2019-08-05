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

static constexpr int kMaxThreadCount = 250; //< More is considered as suspicious.

} // namespace

struct DebugInfoInstrument::Private
{
    const bool profilerMode = ini().profilerMode;
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
    std::optional<int> gdiHandleCount;
    std::optional<int> userHandleCount;
    std::optional<int> threadCount;

    QString highlightedLine(const QString& line) const
    {
        return QString("<span style=\"font-weight: bold; color: red;\">%1</span>").arg(line);
    }

    void updateHandles()
    {
        if (!profilerMode)
            return;
        #if defined(Q_OS_WIN)
            const auto processHandle = GetCurrentProcess();
            gdiHandleCount = GetGuiResources(processHandle, GR_GDIOBJECTS);
            userHandleCount = GetGuiResources(processHandle, GR_USEROBJECTS);
        #endif
    }

    void updateThreads()
    {
        if (!profilerMode)
            return;
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
            if (gdiHandleCount)
                debugInfoLines.append(QString("GDI: %1").arg(*gdiHandleCount));
            if (userHandleCount)
                debugInfoLines.append(QString("User: %1").arg(*userHandleCount));
            if (threadCount)
            {
                auto threadsLine = QString("Threads: %1").arg(*threadCount);
                if (*threadCount > kMaxThreadCount)
                    threadsLine = highlightedLine(threadsLine);
                debugInfoLines.append(threadsLine);
            }
        }
        return debugInfoLines.join("<br>");
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

    if (d->logTimer.hasExpired(kLogInterval.count()))
    {
        if (d->gdiHandleCount || d->userHandleCount || d->threadCount)
        {
            QStringList logMessageLines;
            if (d->gdiHandleCount)
                logMessageLines.append(QString("GDI: %1").arg(*d->gdiHandleCount));
            if (d->userHandleCount)
                logMessageLines.append(QString("User: %1").arg(*d->userHandleCount));
            if (d->threadCount)
                logMessageLines.append(QString("Threads: %1").arg(*d->threadCount));

            NX_INFO(this, lm(QString("Client profiling info: %1")
                .arg(logMessageLines.join(", "))));
            d->logTimer.restart();
        }
    }

    if (changed)
        emit debugInfoChanged(d->debugInfoRichText());

    return false;
}

} // namespace nx::vms::client::desktop
