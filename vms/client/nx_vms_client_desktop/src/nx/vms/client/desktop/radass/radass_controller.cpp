// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "radass_controller.h"

#include <client/client_runtime_settings.h>
#include <nx/fusion/serialization/lexical_functions.h>
#include <nx/utils/guarded_callback.h>
#include <nx/utils/log/assert.h>
#include <nx/utils/log/log.h>
#include <nx/utils/thread/mutex.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/camera/abstract_video_display.h>
#include <nx/vms/client/desktop/debug_utils/utils/performance_monitor.h>
#include <nx/vms/client/desktop/ini.h>
#include <nx/vms/client/desktop/state/shared_memory_manager.h>
#include <utils/common/counter_hash.h>

#include "radass_controller_params.h"
#include "radass_support.h"
#include "radass_types.h"
#include "utils/qt_timers.h"

using namespace std::chrono;
using namespace nx::vms::client::desktop;
using namespace radass;

namespace {

bool isForcedHqDisplay(AbstractVideoDisplay* display)
{
    return display->isFullScreen() || display->isZoomWindow() || display->isFisheyeEnabled();
}

bool isFastForwardOrRevMode(float speed)
{
    return speed > 1 + kSpeedValueEpsilon || speed < 0;
}

bool isFastForwardOrRevMode(AbstractVideoDisplay* display)
{
    return isFastForwardOrRevMode(display->getSpeed());
}

bool isSmallItem(AbstractVideoDisplay* display)
{
    QSize sz = display->getMaxScreenSize();
    return sz.height() <= kLowQualityScreenSize.height();
}

bool isSmallOrMediumItem(AbstractVideoDisplay* display)
{
    QSize sz = display->getMaxScreenSize();
    return sz.height() <= kLowQualityScreenSize.height() * kLqToHqSizeThreshold;
}

bool isBigItem(AbstractVideoDisplay* display)
{
    return !isSmallOrMediumItem(display);
}

bool itemQualityCanBeRaised(AbstractVideoDisplay* display)
{
    return !isSmallItem(display);
}

bool itemQualityCanBeLowered(AbstractVideoDisplay* display)
{
    return !isForcedHqDisplay(display);
}

bool calculateConsiderOverallCpuUsage()
{
    enum class Mode { disable, automatic, enable };
    static const QString kAutoValue = "auto";
    Mode iniValue =
        []() -> Mode
        {
            if (kAutoValue == ini().considerOverallCpuUsageInRadass)
                return Mode::automatic;

            return QnLexical::deserialized<bool>(ini().considerOverallCpuUsageInRadass)
                ? Mode::enable
                : Mode::disable;
        }();

    switch (iniValue)
    {
        case Mode::disable:
            return false;
        case Mode::enable:
            return true;
        default: //< Automatic mode.
            break;
    }

    return appContext()->runtimeSettings()->isVideoWallMode()
        || !appContext()->sharedMemoryManager()->isSingleInstance();
}

enum class LqReason
{
    none,
    smallItem,
    performance,
    performanceInFf,
    tooManyItems,
    cpuUsage,
    manual,
};

enum class FindMethod
{
    Biggest,
    Smallest
};

struct ConsumerInfo
{
    using Mode = RadassMode;

    AbstractVideoDisplay* display = nullptr;

    float toLqSpeed = 0.0;
    LqReason lqReason = LqReason::none;
    ElapsedTimerPtr initialTime;
    ElapsedTimerPtr awaitingLqTime;

    // Item will go to LQ if it is small for this period of time already.
    ElapsedTimerPtr itemIsSmallInHq;

    // Custom mode means unsupported consumer.
    Mode mode = Mode::Auto;

    /** Previous mode for scenario when support is disabled and then enabled again. */
    Mode previousMode = Mode::Auto;

    // Required for std::vector resize.
    ConsumerInfo() = default;

    ConsumerInfo(AbstractVideoDisplay* display, const TimerFactoryPtr& timerFactory):
        display(display),
        initialTime(timerFactory->createElapsedTimer()),
        awaitingLqTime(timerFactory->createElapsedTimer()),
        itemIsSmallInHq(timerFactory->createElapsedTimer()),
        m_name(display->getName())
    {
        initialTime->start();
        if (!display->isRadassSupported())
            mode = Mode::Custom;
    }

    QString toString() const
    {
        return nx::format("%1 (%2)").arg(m_name).arg(QString::number(qint64(this), 16));
    }

private:
    QString m_name;
};

} // namespace

namespace nx::vms::client::desktop {

struct RadassController::Private
{
    RadassController* const q;

    mutable nx::Mutex mutex;

    // Time factory should be declared before ALL timers to prevent crash
    // as it is needed in theirs destructors.
    TimerFactoryPtr timerFactory;

    TimerPtr mainTimer;
    ElapsedTimerPtr lastAutoSwitchTimer; //< Latest automatic HQ->LQ or LQ->HQ switch.
    int timerTicks = 0;    //< onTimer ticks count
    ElapsedTimerPtr lastSystemRtspDrop; //< Time since last RTSP drop in the system.

    // If some item has performance problems second time, block auto-hq transition for all cameras.
    bool autoHqTransitionAllowed = true;

    // When something actually changed the last time (for performance warning only).
    ElapsedTimerPtr lastModeChangeTimer;

    float lastSystemCpuLoadValue = 0.0;
    float lastProcessCpuLoadValue = 0.0;

    /** Timer started when overall CPU usage surpasses the limit. Reset when CPU usage drops. */
    ElapsedTimerPtr cpuIssueTimer;

    std::vector<ConsumerInfo> consumers;
    using Consumer = decltype(consumers)::iterator;

    QnCounterHash<AbstractVideoDisplay::CameraID> cameras;

    bool considerOverallCpuUsage = false;
    TimerPtr overallCpuUsageModeTimer;

    Private(RadassController* owner, const TimerFactoryPtr& timerFactory):
        q(owner),
        mutex(nx::Mutex::Recursive),
        mainTimer(timerFactory->createTimer()),
        lastAutoSwitchTimer(timerFactory->createElapsedTimer()),
        lastSystemRtspDrop(timerFactory->createElapsedTimer()),
        lastModeChangeTimer(timerFactory->createElapsedTimer()),
        cpuIssueTimer(timerFactory->createElapsedTimer()),
        overallCpuUsageModeTimer(timerFactory->createTimer())
    {
        this->timerFactory = timerFactory;
        lastAutoSwitchTimer->start();
        lastModeChangeTimer->start();

        QObject::connect(mainTimer.get(), &AbstractTimer::timeout, q, &RadassController::onTimer);
        mainTimer->start(kTimerInterval);

        QObject::connect(overallCpuUsageModeTimer.get(), &AbstractTimer::timeout, q,
            [this]() { updateOverallCpuUsageMode(); });
        overallCpuUsageModeTimer->start(kUpdateCpuUsageModeInterval);
        updateOverallCpuUsageMode();
    }

    Consumer findByDisplay(AbstractVideoDisplay* display)
    {
        return std::find_if(consumers.begin(), consumers.end(),
            [display](const ConsumerInfo& info)
            {
                return info.display == display;
            });
    }

    bool isValid(Consumer consumer) const
    {
        return consumer != consumers.end();
    }

    void updateOverallCpuUsageMode()
    {
        bool value = calculateConsiderOverallCpuUsage();
        if (value == considerOverallCpuUsage)
            return;

        considerOverallCpuUsage = value;
        if (value)
        {
            NX_VERBOSE(this, "Enable overall performance monitor. Parameters:");
            NX_VERBOSE(this, "High System CPU: %1", ini().highSystemCpuUsageInRadass);
            NX_VERBOSE(this, "Critical System CPU: %1", ini().criticalSystemCpuUsageInRadass);
            NX_VERBOSE(this, "High Process CPU: %1", ini().highProcessCpuUsageInRadass);
            NX_VERBOSE(this, "Critical Process CPU: %1", ini().criticalProcessCpuUsageInRadass);
            QObject::connect(appContext()->performanceMonitor(),
                &PerformanceMonitor::valuesChanged,
                q,
                [this](const QVariantMap& values)
                {
                    const QVariant totalCpuValue = values.value(PerformanceMonitor::kTotalCpu);
                    lastSystemCpuLoadValue = totalCpuValue.toFloat();
                    const QVariant processCpuValue = values.value(PerformanceMonitor::kProcessCpu);
                    lastProcessCpuLoadValue = processCpuValue.toFloat();
                    NX_VERBOSE(this, "Overall performance: total CPU %1, process CPU %2",
                        lastSystemCpuLoadValue, lastProcessCpuLoadValue);
                });
        }
        else
        {
            NX_VERBOSE(this, "Disable overall performance monitor.");
            appContext()->performanceMonitor()->disconnect(q);
        }
        appContext()->performanceMonitor()->setEnabled(value);
    }

    /** Quality cannot be raised as CPU usage will probably overflow the system limit. */
    bool cpuUsageIsTooHighToRaiseQuality() const
    {
        if (!considerOverallCpuUsage)
            return false;

        const bool result = lastSystemCpuLoadValue > ini().highSystemCpuUsageInRadass
            || lastProcessCpuLoadValue > ini().highProcessCpuUsageInRadass;
        if (result)
            NX_VERBOSE(this, "Overall CPU usage is too high to raise quality.");

        return result;
    }

    /** Overall CPU usage is too high, quality should be lowered. */
    bool cpuUsageIsCritical() const
    {
        if (!considerOverallCpuUsage)
            return false;

        const bool result = lastSystemCpuLoadValue > ini().criticalSystemCpuUsageInRadass
            || lastProcessCpuLoadValue > ini().criticalProcessCpuUsageInRadass;
        if (result)
            NX_VERBOSE(this, "Overall CPU usage is critical.");

        return result;
    }

    using SearchCondition = std::function<bool(AbstractVideoDisplay*)>;
    Consumer findConsumer(FindMethod method,
        MediaQuality findQuality,
        SearchCondition condition = SearchCondition(),
        int* displaySize = nullptr)
    {
        const bool findHq = findQuality == MEDIA_Quality_High;
        if (displaySize)
            *displaySize = 0;

        QMultiMap<qint64, Consumer> consumerByScreenSize;
        for (auto info = consumers.begin(); info != consumers.end(); ++info)
        {
            AbstractVideoDisplay* display = info->display;

            // Skip unsupported cameras and cameras with manual stream control.
            if (info->mode != RadassMode::Auto)
                continue;

            // Skip cameras that do not fit condition (if any provided).
            if (condition && !condition(display))
                continue;

            const QSize size = display->getMaxScreenSize();
            const qint64 screenSquare = size.width() * size.height();

            static const int kMaximumValue = std::numeric_limits<int>::max();

            // Put display with max pps to the beginning, so if slow stream do HQ->LQ for max pps
            // display (between displays with same size).
            const QSize res = display->getVideoSize();
            const int pps = kMaximumValue - (res.width() * res.height());

            consumerByScreenSize.insert((screenSquare << 32) + pps, info);
        }

        QMultiMapIterator<qint64, Consumer> itr(consumerByScreenSize);
        if (method == FindMethod::Biggest)
            itr.toBack();

        while ((method == FindMethod::Smallest && itr.hasNext())
            || (method == FindMethod::Biggest && itr.hasPrevious()))
        {
            if (method == FindMethod::Smallest)
                itr.next();
            else
                itr.previous();

            const auto consumer = itr.value();
            const auto display = consumer->display;
            const bool isReaderHq = display->getQuality() == MEDIA_Quality_High;
            if (isReaderHq == findHq)
            {
                if (displaySize)
                    *displaySize = itr.key() >> 32;
                return consumer;
            }
        }

        return consumers.end();
    }

    void gotoLowQuality(Consumer consumer, LqReason reason, double speed = kAutomaticSpeed)
    {
        const bool isInHiQuality = consumer->display->getQuality() == MEDIA_Quality_High;
        const auto consumerSpeed = speed != kAutomaticSpeed ? speed : consumer->display->getSpeed();

        const auto lQReasonString = [reason, consumer]() -> QString
            {
                const auto display = consumer->display;
                const QString performanceProblem = display->dataQueueSize() < 3
                    ? lit(" (Network)")
                    : lit(" (CPU)");
                switch (reason)
                {
                    case LqReason::smallItem:
                        return "smallItem";
                    case LqReason::performance:
                        return "performance" + performanceProblem;
                    case LqReason::performanceInFf:
                        return "performanceInFf" + performanceProblem;
                    case LqReason::tooManyItems:
                        return "tooManyItems";
                    case LqReason::cpuUsage:
                        return "cpuUsage";
                    case LqReason::manual:
                        return "manual";
                    default:
                        break;
                }
                return QString();
            };

        if (isInHiQuality)
        {
            const auto oldReason = consumer->lqReason;
            const auto wasPerformanceProblem = oldReason == LqReason::performance
                || oldReason == LqReason::tooManyItems;

            // Do not block Lq-Hq transition if current consumer was switched to Lq while FF/REW.
            if (wasPerformanceProblem && reason == LqReason::performance)
            {
                // Item goes to LQ again because of performance issue.
                autoHqTransitionAllowed = false;
                NX_VERBOSE(this,
                    "Performance problem repeated on consumer %1, blocking LQ->HQ", *consumer);
            }

            NX_VERBOSE(this, "Switching High->Low, %1 ms since last automatic change, reason %2",
                lastAutoSwitchTimer->elapsedMs(),
                lQReasonString());

            // Last actual change time (for performance warning only).
            lastModeChangeTimer->restart();
        }

        consumer->display->setQuality(MEDIA_Quality_Low, true);
        consumer->lqReason = reason;
        consumer->toLqSpeed = consumerSpeed;
        consumer->awaitingLqTime->invalidate();
    }

    void gotoHighQuality(Consumer consumer)
    {
        if (consumer->display->getQuality() != MEDIA_Quality_High)
        {
            NX_VERBOSE(this, "Switching Low->High, %1 ms since last automatic change",
                lastAutoSwitchTimer->elapsed().count());

            // Last actual change time (for performance warning only).
            lastModeChangeTimer->restart();
        }

        consumer->display->setQuality(MEDIA_Quality_High, true);
    }

    bool existsBufferingDisplay() const
    {
        return std::any_of(consumers.cbegin(), consumers.cend(),
            [](const ConsumerInfo& info) { return info.display->isBuffering(); });
    }

    Consumer slowConsumer()
    {
        return std::find_if(consumers.begin(), consumers.end(),
            [](const ConsumerInfo& info)
            {
                return info.mode == RadassMode::Auto
                    && info.display->getQuality() == MEDIA_Quality_Low
                    && info.lqReason == LqReason::performance;
            });
    }

    void optimizeConsumerQuality(Consumer consumer)
    {
        NX_ASSERT(consumer->mode == RadassMode::Auto);
        // Do not handle recently added items, some start animation can be in progress.
        if (!consumer->initialTime->hasExpired(kRecentlyAddedInterval))
            return;

        const auto display = consumer->display;

        if (isForcedHqDisplay(display))
        {
            if (display->getQuality() != MEDIA_Quality_High)
                NX_VERBOSE(this, "Forced switch %1 to HQ", *consumer);
            gotoHighQuality(consumer);
            return;
        }

        // Switch HQ->LQ if visual item size is small.
        if (display->getQuality() == MEDIA_Quality_High
            && isSmallItem(display)
            && !display->isMediaPaused())
        {
            // Run timer for the small items.
            if (!consumer->itemIsSmallInHq->isValid())
                consumer->itemIsSmallInHq->start();

            if (consumer->itemIsSmallInHq->hasExpired(kLowerSmallItemQualityInterval))
            {
                NX_VERBOSE(this, "%1 was small in HQ for 1 second", *consumer);
                gotoLowQuality(consumer, LqReason::smallItem);
                addHqTry();
            }
        }
        else
        {
            // Item is not small or not in HQ or not playing anymore.
            consumer->itemIsSmallInHq->invalidate();
        }

        // switch LQ->HQ for LIVE here.
        if (display->isRealTimeSource()
            // LQ live stream.
            && display->getQuality() == MEDIA_Quality_Low
            // Check if camera is not out of the screen
            && !display->isPaused()
            // Check if camera is not paused
            && !display->isMediaPaused())
        {
            NX_VERBOSE(this, "Timer check: there is a live camera in LQ");

            // There are no a lot of packets in the queue (it is possible if CPU slow).
            const bool decodingIsOk =
                display->dataQueueSize() <= (display->maxDataQueueSize() * 0.75);

            // No recently slow report by any camera.
            const bool connectionIsOk =
                lastSystemRtspDrop->hasExpiredOrInvalid(kQualitySwitchInterval);

            // Is big enough item for HQ.
            const bool sizeIsOk = isBigItem(display);

            const bool canRaiseQualityByCpu = !cpuUsageIsTooHighToRaiseQuality();

            if (decodingIsOk && connectionIsOk && sizeIsOk && canRaiseQualityByCpu)
            {
                NX_VERBOSE(this, "%1 can be switched to HQ safely, everything is OK", *consumer);
                streamBackToNormal(consumer);
            }
            else
            {
                static constexpr std::array<std::string_view, 2> isOk{"Issue", "OK"};
                NX_VERBOSE(this, "Decoding: %1, rtsp: %2, size: %3, cpu: %4",
                    isOk[decodingIsOk],
                    isOk[connectionIsOk],
                    isOk[sizeIsOk],
                    isOk[canRaiseQualityByCpu]);
            }
        }

        if (consumer->awaitingLqTime->isValid()
            && consumer->awaitingLqTime->hasExpired(kQualitySwitchInterval))
        {
            NX_VERBOSE(this, "%1 had slow stream for 5 seconds", *consumer);
            gotoLowQuality(consumer, LqReason::performance);
        }
    }

    void setupNewConsumer(Consumer consumer)
    {
        if (consumer->mode != RadassMode::Auto)
            return;

        // Allow LQ->HQ switching.
        autoHqTransitionAllowed = true;
        NX_VERBOSE(this, "Setup new consumer: %1", *consumer);

        if (isForcedHqDisplay(consumer->display))
        {
            NX_VERBOSE(this, "Forcing HQ");
            gotoHighQuality(consumer);
            return;
        }

        // If there are a lot of cameras on the layout, move all cameras to low quality if possible.
        if (consumers.size() > kMaximumConsumersCount)
        {
            NX_VERBOSE(this, "Too many cameras on the scene, going to LQ");
            gotoLowQuality(consumer, LqReason::tooManyItems);
            return;
        }

        // If there are at least one slow camera, make newly added camera low quality.
        const auto slow = slowConsumer();
        if (isValid(slow))
        {
            NX_VERBOSE(this, "There is slow consumer %1, going to LQ", *slow);
            gotoLowQuality(consumer, slow->lqReason);
        }
    }

    void onSlowStream(Consumer consumer)
    {
        NX_VERBOSE(this, "Slow stream on %1 detected", *consumer);
        lastSystemRtspDrop->start();

        // Skip unsupported cameras and cameras with manual stream control.
        if (consumer->mode != RadassMode::Auto)
            return;

        const auto display = consumer->display;

        if (isForcedHqDisplay(display))
        {
            NX_VERBOSE(this, "%1 is in forced HQ, leaving as is.", *consumer);
            return;
        }

        // Check if reader already at LQ.
        if (display->getQuality() == MEDIA_Quality_Low)
            return;

        // If item is in high speed, switch it to Lq immediately without triggering timer. Speed is
        // saved to an external variable to avoid race condition @rvasilenko.
        const auto speed = display->getSpeed();
        if (isFastForwardOrRevMode(speed))
        {
            NX_VERBOSE(this, "%1 is in FF/Rew, swithing to LQ immediately.", *consumer);
            gotoLowQuality(consumer, LqReason::performanceInFf, speed);
            return;
        }

        // Do not go to LQ if recently switch occurred.
        if (!lastAutoSwitchTimer->hasExpired(kQualitySwitchInterval))
        {
            NX_VERBOSE(this,
                "Quality switch was less than 5 seconds ago, leaving %1 as is.", *consumer);
            consumer->awaitingLqTime->restart();
            return;
        }

        auto smallestConsumer = findConsumer(
            FindMethod::Smallest,
            MEDIA_Quality_High,
            itemQualityCanBeLowered);
        NX_ASSERT(isValid(smallestConsumer), "At least one camera must be found");
        if (!isValid(smallestConsumer))
        {
            NX_VERBOSE(this, "Smallest HQ camera not found, using source %1 instead.", *consumer);
            smallestConsumer = consumer;
        }
        NX_VERBOSE(this, "Finding smallest HQ camera %1 and switching it to LQ.", *smallestConsumer);
        gotoLowQuality(smallestConsumer, LqReason::performance);
        lastAutoSwitchTimer->restart();
    }

    void streamBackToNormal(Consumer consumer)
    {
        // Camera do not have slow stream anymore.
        consumer->awaitingLqTime->invalidate();

        // Skip unsupported cameras and cameras with manual stream control.
        if (consumer->mode != RadassMode::Auto)
            return;

        AbstractVideoDisplay* display = consumer->display;
        if (consumer->lqReason == LqReason::performanceInFf)
        {
            const float oldSpeed = consumer->toLqSpeed;
            const float currentSpeed = display->getSpeed();
            if (!isFastForwardOrRevMode(currentSpeed)
                || qAbs(currentSpeed) < qAbs(oldSpeed))
            {
                // If item leave high speed mode change it back to HQ.
                NX_VERBOSE(this, "%1 left FF mode or slowed, switching to HQ", *consumer);
                gotoHighQuality(consumer);
                return;
            }
        }

        if (isFastForwardOrRevMode(display))
        {
            NX_VERBOSE(this, "%1 still in FF mode, left in LQ", *consumer);
            return;
        }

        // Auto-Hq transition is not blocked
        if (!autoHqTransitionAllowed)
        {
            NX_VERBOSE(this, "Auto-Hq transition of %1 is blocked", *consumer);
            return;
        }

        // Do not try HQ for small items.
        if (isSmallOrMediumItem(display))
        {
            NX_VERBOSE(this, "%1 is not big enough", *consumer);
            return;
        }

        // If item go to LQ because of small, return to HQ without delay.
        if (consumer->lqReason == LqReason::smallItem)
        {
            NX_VERBOSE(this, "%1 was small, now big enough, go HQ", *consumer);
            gotoHighQuality(consumer);
            return;
        }

        // Try one more LQ->HQ switch.

        // Recently LQ->HQ or HQ->LQ
        if (!lastAutoSwitchTimer->hasExpired(kQualitySwitchInterval))
        {
            NX_VERBOSE(this,
                "Quality switch was less than 5 seconds ago, leaving %1 as is.", *consumer);
            return;
        }

        // Recently slow report received (not all reports affect d->lastAutoSwitchTimer).
        if (!lastSystemRtspDrop->hasExpiredOrInvalid(kQualitySwitchInterval))
        {
            NX_VERBOSE(this,
                "RTSP drop on %1 was less than 5 seconds ago, leaving as is.", *consumer);
            return;
        }

        // Do not go to HQ if some display perform opening.
        if (existsBufferingDisplay())
        {
            NX_VERBOSE(this, "Some item is opening right now, leaving %1 as is.", *consumer);
            return;
        }

        NX_VERBOSE(this, "Switching %1 to hi quality", *consumer);
        gotoHighQuality(consumer);
        lastAutoSwitchTimer->restart();
    }

    // Try LQ->HQ once more for all cameras.
    void addHqTry()
    {
        NX_VERBOSE(this, "Try LQ->HQ once more for all cameras");
        autoHqTransitionAllowed = true;
    }

    // Rearrange items quality: put small items to LQ state, large to HQ.
    void optimizeItemsQualityBySize()
    {
        // Do not optimize quality if recently switch occurred.
        if (!lastAutoSwitchTimer->hasExpired(kQualitySwitchInterval))
            return;

        // Do not rearrange items if any item is in FF/REW mode right now.
        if (std::any_of(consumers.cbegin(), consumers.cend(),
            [](const ConsumerInfo& consumer) { return isFastForwardOrRevMode(consumer.display); }))
        {
            return;
        }

        // Find the biggest camera with auto control and low quality.
        int largeSize = 0;
        const auto largeConsumer = findConsumer(FindMethod::Biggest,
            MEDIA_Quality_Low,
            itemQualityCanBeRaised,
            &largeSize);
        if (!isValid(largeConsumer))
            return;

        // Find the smallest camera with auto control and hi quality.
        int smallSize = 0;
        const auto smallConsumer = findConsumer(FindMethod::Smallest,
            MEDIA_Quality_High,
            itemQualityCanBeLowered,
            &smallSize);
        if (!isValid(smallConsumer))
            return;

        if (largeSize >= smallSize * 2)
        {
            // swap items quality
            NX_VERBOSE(this, "Swap items quality");
            NX_VERBOSE(this, "Smaller %1 goes to LQ", *smallConsumer);
            gotoLowQuality(smallConsumer, largeConsumer->lqReason);
            NX_VERBOSE(this, "Bigger %1 goes to HQ", *largeConsumer);
            gotoHighQuality(largeConsumer);
            lastAutoSwitchTimer->restart();
        }
    }

    /**
     * Check whether system or process CPU usage is too high for some time and lower item quality
     * in this case.
     */
    void optimizeItemsQualityByCpuUsage()
    {
        if (!considerOverallCpuUsage)
            return;

        // If everything is OK, turn off the timer.
        if (!cpuUsageIsCritical())
        {
            if (cpuIssueTimer->isValid())
                NX_VERBOSE(this, "CPU usage is back to normal");
            cpuIssueTimer->invalidate();
            return;
        }

        // On first issue starting timer.
        if (!cpuIssueTimer->isValid())
        {
            NX_VERBOSE(this, "CPU usage is critical, starting timer");
            cpuIssueTimer->restart();
        }

        if (cpuIssueTimer->hasExpired(milliseconds(ini().criticalRadassCpuUsageTimeMs)))
        {
            NX_VERBOSE(this, "CPU usage is critical for some time already, switch item to LQ");

            // Do not go to LQ if recently switch occurred.
            if (!lastAutoSwitchTimer->hasExpired(kQualitySwitchInterval))
            {
                NX_VERBOSE(this, "Quality switch was less than 5 seconds ago");
                return;
            }

            // Find the smallest consumer which is in Auto mode and in HQ and switch it to LQ.
            auto smallestConsumer = findConsumer(
                FindMethod::Smallest,
                MEDIA_Quality_High,
                itemQualityCanBeLowered);

            if (isValid(smallestConsumer))
            {
                gotoLowQuality(smallestConsumer, LqReason::cpuUsage);
                cpuIssueTimer->restart();
            }
            else
            {
                NX_VERBOSE(this, "Cannot find an item to lower it's quality.");
            }
        }
    }

    void adaptToConsumerChanges(AbstractVideoDisplay* display)
    {
        auto consumer = findByDisplay(display);
        if (!isValid(consumer))
            return;

        const bool isSupported = consumer->display->isRadassSupported();
        const bool wasSupported = consumer->mode != RadassMode::Custom;
        if (isSupported == wasSupported)
            return;

        NX_ASSERT(consumer->previousMode != RadassMode::Custom, "Should never get this");
        if (isSupported)
        {
            consumer->mode = consumer->previousMode;
            setupNewConsumer(consumer);
        }
        else
        {
            consumer->previousMode = consumer->mode;
            consumer->mode = RadassMode::Custom;
        }
    }
};

// If no timer factory is provided, use default qt-based one (production mode).
RadassController::RadassController(QObject* parent):
    RadassController(TimerFactoryPtr(new QtTimerFactory()), parent)
{
}

RadassController::RadassController(TimerFactoryPtr timerFactory, QObject* parent):
    base_type(parent),
    d(new Private(this, timerFactory))
{

}

RadassController::~RadassController()
{
}

void RadassController::onSlowStream(AbstractVideoDisplay* display)
{
    NX_MUTEX_LOCKER lock(&d->mutex);

    auto consumer = d->findByDisplay(display);
    if (!d->isValid(consumer))
        return;

    d->onSlowStream(consumer);
}

void RadassController::streamBackToNormal(AbstractVideoDisplay* display)
{
    NX_MUTEX_LOCKER lock(&d->mutex);

    if (display->getQuality() == MEDIA_Quality_High)
        return; // reader already at HQ

    auto consumer = d->findByDisplay(display);
    if (!d->isValid(consumer))
        return;

    NX_VERBOSE(this, "%1 stream back to normal.", *consumer);
    d->streamBackToNormal(consumer);
}

void RadassController::onTimer()
{
    NX_MUTEX_LOCKER lock(&d->mutex);

    if (++d->timerTicks >= kAdditionalHQRetryCounter)
    {
        // Sometimes allow addition LQ->HQ switch.
        // Check if there were no slow stream reports lately.
        if (d->lastSystemRtspDrop->hasExpiredOrInvalid(kQualitySwitchInterval))
        {
            d->addHqTry();
            d->timerTicks = 0;
        }
    }

    for (auto consumer = d->consumers.begin(); consumer != d->consumers.end(); ++consumer)
    {
        switch (consumer->mode)
        {
            case RadassMode::High:
            {
                d->gotoHighQuality(consumer);
                break;
            }
            case RadassMode::Low:
            {
                if (isForcedHqDisplay(consumer->display))
                    d->gotoHighQuality(consumer);
                else
                    d->gotoLowQuality(consumer, LqReason::manual);
                break;
            }
            case RadassMode::Custom:
            {
                // Skip unsupported cameras.
                break;
            }
            case RadassMode::Auto:
            {
                d->optimizeConsumerQuality(consumer);
                break;
            }
            default:
                break;
        }
    }

    d->optimizeItemsQualityBySize();
    d->optimizeItemsQualityByCpuUsage();

    if (d->lastModeChangeTimer->hasExpired(kPerformanceLossCheckInterval))
    {
        // Recently slow report received or there is a slow consumer.
        const auto performanceLoss =
            (!d->lastSystemRtspDrop->hasExpiredOrInvalid(kPerformanceLossCheckInterval))
            || d->isValid(d->slowConsumer());

        const auto hasHq = std::any_of(d->consumers.cbegin(), d->consumers.cend(),
            [](const ConsumerInfo& info) { return info.mode == RadassMode::High; });

        if (hasHq && performanceLoss && !d->existsBufferingDisplay())
            emit performanceCanBeImproved();

        d->lastModeChangeTimer->restart();
    }
}

int RadassController::consumerCount() const
{
    NX_MUTEX_LOCKER lock(&d->mutex);
    return (int) d->consumers.size();
}

void RadassController::registerConsumer(AbstractVideoDisplay* display)
{
    NX_MUTEX_LOCKER lock(&d->mutex);

    d->consumers.push_back(ConsumerInfo(display, d->timerFactory));
    auto consumer = d->consumers.end() - 1;
    NX_ASSERT(consumer->display == display);

    d->setupNewConsumer(consumer);
    d->lastModeChangeTimer->restart();

    // Listen to camera changes to make sure second stream will start working as soon as possible.
    display->setCallbackForStreamChanges(nx::utils::guarded(this,
        [this, display]
        {
            d->adaptToConsumerChanges(display);
        }));
    if (const auto camera = display->getCameraID(); !camera.isNull())
        d->cameras.insert(camera);
}

void RadassController::unregisterConsumer(AbstractVideoDisplay* display)
{
    NX_MUTEX_LOCKER lock(&d->mutex);

    auto consumer = d->findByDisplay(display);
    if (!d->isValid(consumer))
        return;

    d->consumers.erase(consumer);
    d->addHqTry();
    d->lastModeChangeTimer->restart();

    display->setCallbackForStreamChanges({});
    if (const auto camera = display->getCameraID(); !camera.isNull())
        d->cameras.remove(camera);
}

RadassMode RadassController::mode(AbstractVideoDisplay* display) const
{
    NX_MUTEX_LOCKER lock(&d->mutex);
    auto consumer = d->findByDisplay(display);
    NX_ASSERT(d->isValid(consumer));
    if (!d->isValid(consumer))
        return RadassMode::Auto;

    return consumer->mode;
}

void RadassController::setMode(AbstractVideoDisplay* display, RadassMode mode)
{
    NX_MUTEX_LOCKER lock(&d->mutex);

    auto consumer = d->findByDisplay(display);
    NX_ASSERT(d->isValid(consumer));
    if (!d->isValid(consumer))
        return;

    if (consumer->mode == mode)
        return;

    NX_ASSERT(consumer->mode != RadassMode::Custom);
    if (consumer->mode == RadassMode::Custom)
        return;

    consumer->mode = mode;

    switch (mode)
    {
        case RadassMode::Auto:
            d->setupNewConsumer(consumer);
            break;
        case RadassMode::High:
            d->gotoHighQuality(consumer);
            break;
        case RadassMode::Low:
            // What if full screen or zoom window?
            d->gotoLowQuality(consumer, LqReason::manual);
            break;
        default:
            NX_ASSERT(false, "Should never get here");
            break;
    }
    d->lastModeChangeTimer->restart();
}

} // namespace nx::vms::client::desktop
