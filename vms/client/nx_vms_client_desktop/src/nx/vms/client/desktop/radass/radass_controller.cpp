#include "radass_controller.h"

#include <QtCore/QTimer>
#include <QtCore/QElapsedTimer>

#include <core/resource/camera_resource.h>

#include <camera/cam_display.h>

#include <nx/streaming/archive_stream_reader.h>
#include <nx/streaming/media_data_packet.h>

#include <nx/utils/log/assert.h>
#include <nx/utils/log/log.h>
#include <nx/utils/thread/mutex.h>
#include <utils/common/counter_hash.h>

#include "radass_types.h"
#include "radass_support.h"

//#define TRACE_RADASS

namespace {

// Time to ignore recently added cameras.
static constexpr int kRecentlyAddedIntervalMs = 1000;

// Delay between quality switching attempts.
static constexpr int kQualitySwitchIntervalMs = 1000 * 5;

// Delay between checks if performance can be improved.
static constexpr int kPerformanceLossCheckIntervalMs = 1000 * 5;

// Put item to LQ if visual size is small.
static const QSize kLowQualityScreenSize(320 / 1.4, 240 / 1.4);

// Put item back to HQ if visual size increased to kLowQualityScreenSize * kLqToHqSizeThreshold.
static constexpr double kLqToHqSizeThreshold = 1.34;

// Try to balance every 500ms
static constexpr int kTimerIntervalMs = 500;

// Every 10 minutes try to raise quality. Value in counter ticks.
static constexpr int kAdditionalHQRetryCounter = 10 * 60 * 1000 / kTimerIntervalMs;

// Ignore cameras which are on Fast Forward
static constexpr double kSpeedValueEpsilon = 0.0001;

// Switch all cameras to LQ if there are more than 16 cameras on the scene.
static constexpr int kMaximumConsumersCount = 16;

// Network considered as slow if there are less than 3 frames in display queue.
static constexpr int kSlowNetworkFrameLimit = 3;

// Item will go to LQ if it is small for this period of time already.
static constexpr int kLowerSmallItemQualityIntervalMs = 1000;

static constexpr int kAutomaticSpeed = std::numeric_limits<int>::max();

bool isForcedHqDisplay(QnCamDisplay* display)
{
    return display->isFullScreen() || display->isZoomWindow() || display->isFisheyeEnabled();
}

bool isFastForwardOrRevMode(float speed)
{
    return speed > 1 + kSpeedValueEpsilon || speed < 0;
}

bool isFastForwardOrRevMode(QnCamDisplay* display)
{
    return isFastForwardOrRevMode(display->getSpeed());
}

QnVirtualCameraResourcePtr getCamera(QnCamDisplay* display)
{
    if (!display)
        return QnVirtualCameraResourcePtr();

    const auto reader = display->getArchiveReader();
    NX_ASSERT(reader);
    if (!reader)
        return QnVirtualCameraResourcePtr();

    return reader->getResource().dynamicCast<QnVirtualCameraResource>();
}

// Omit cameras without dual streaming.
bool isSupportedDisplay(QnCamDisplay* display)
{
    return nx::vms::client::desktop::isRadassSupported(getCamera(display));
}

bool isSmallItem(QnCamDisplay* display)
{
    QSize sz = display->getMaxScreenSize();
    return sz.height() <= kLowQualityScreenSize.height();
}

bool isSmallOrMediumItem(QnCamDisplay* display)
{
    QSize sz = display->getMaxScreenSize();
    return sz.height() <= kLowQualityScreenSize.height() * kLqToHqSizeThreshold;
}

bool isBigItem(QnCamDisplay* display)
{
    return !isSmallOrMediumItem(display);
}

bool itemQualityCanBeRaised(QnCamDisplay* display)
{
    return !isSmallItem(display);
}

bool itemQualityCanBeLowered(QnCamDisplay* display)
{
    return !isForcedHqDisplay(display);
}

enum class LqReason
{
    none,
    smallItem,
    performance,
    performanceInFf,
    tooManyItems
};

enum class FindMethod
{
    Biggest,
    Smallest
};

struct ConsumerInfo
{
    using Mode = nx::vms::client::desktop::RadassMode;

    QnCamDisplay* display = nullptr;

    float toLqSpeed = 0.0;
    LqReason lqReason = LqReason::none;
    QElapsedTimer initialTime;
    QElapsedTimer awaitingLqTime;

    // Item will go to LQ if it is small for this period of time already.
    QElapsedTimer itemIsSmallInHq;

    // Custom mode means unsupported consumer.
    Mode mode = Mode::Auto;

    // Required for std::vector resize.
    ConsumerInfo() = default;

    ConsumerInfo(QnCamDisplay* display):
        display(display),
        m_name(display->resource()->toResourcePtr()->getName())
    {
        initialTime.start();
        if (!isSupportedDisplay(display))
            mode = Mode::Custom;
    }

    QString toString() const
    {
        return lm("%1 (%2)").arg(m_name).arg(QString::number(qint64(this), 16));
    }

private:
    QString m_name;
};

} // namespace

namespace nx::vms::client::desktop {

struct RadassController::Private
{
    mutable QnMutex mutex;
    QElapsedTimer lastAutoSwitchTimer; //< Latest automatic HQ->LQ or LQ->HQ switch.
    int timerTicks = 0;    //< onTimer ticks count
    QElapsedTimer lastSystemRtspDrop; //< Time since last RTSP drop in the system.

    // If some item has performance problems second time, block auto-hq transition for all cameras.
    bool autoHqTransitionAllowed = true;

    // When something actually changed the last time (for performance warning only).
    QElapsedTimer lastModeChangeTimer;

    std::vector<ConsumerInfo> consumers;
    using Consumer = decltype(consumers)::iterator;

    QnCounterHash<QnVirtualCameraResourcePtr> cameras;

    Private():
        mutex(QnMutex::Recursive)
    {
        lastAutoSwitchTimer.start();
        lastModeChangeTimer.start();
    }

    template<typename T>
    void trace(T message, const Consumer consumer)
    {
        if (consumer == consumers.end())
        {
            NX_VERBOSE(this) << message;
            #if defined(TRACE_RADASS)
                qDebug() << message;
            #endif
        }
        else
        {
            NX_VERBOSE(this) << message << *consumer;
            #if defined(TRACE_RADASS)
                qDebug() << message << consumer->toString();
            #endif
        }
    }

    template<typename T>
    void trace(T message)
    {
        trace(message, consumers.end());
    }

    Consumer findByReader(QnArchiveStreamReader* reader)
    {
        return std::find_if(consumers.begin(), consumers.end(),
            [reader](const ConsumerInfo& info)
            {
                return info.display->getArchiveReader() == reader;
            });
    }

    Consumer findByDisplay(QnCamDisplay* display)
    {
        return std::find_if(consumers.begin(), consumers.end(),
            [display](const ConsumerInfo& info)
            {
                return info.display == display;
            });
    }

    bool isValid(Consumer consumer)
    {
        return consumer != consumers.end();
    }

    using SearchCondition = std::function<bool(QnCamDisplay*)>;
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
            QnCamDisplay* display = info->display;

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

        QMapIterator<qint64, Consumer> itr(consumerByScreenSize);
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
            QnArchiveStreamReader* reader = display->getArchiveReader();
            const bool isReaderHq = reader->getQuality() == MEDIA_Quality_High;
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
        const auto reader = consumer->display->getArchiveReader();
        const bool isInHiQuality = reader->getQuality() == MEDIA_Quality_High;
        const auto consumerSpeed = speed != kAutomaticSpeed ? speed : consumer->display->getSpeed();

        const auto lQReasonString = [reason, consumer]
            {
                const auto display = consumer->display;
                const QString performanceProblem = display->queueSize() < 3
                    ? lit(" (Network)")
                    : lit(" (CPU)");
                switch (reason)
                {
                    case LqReason::smallItem:
                        return lit("smallItem");
                    case LqReason::performance:
                        return lit("performance") + performanceProblem;
                    case LqReason::performanceInFf:
                        return lit("performanceInFf") + performanceProblem;
                    case LqReason::tooManyItems:
                        return lit("tooManyItems");
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
                trace("Performance problem repeated on consumer, blocking LQ->HQ", consumer);
            }

            trace(lit("Switching High->Low, %1 ms since last automatic change, reason %2")
                .arg(lastAutoSwitchTimer.elapsed())
                .arg(lQReasonString()));

            // Last actual change time (for performance warning only).
            lastModeChangeTimer.restart();
        }

        reader->setQuality(MEDIA_Quality_Low, true);
        consumer->lqReason = reason;
        consumer->toLqSpeed = consumerSpeed;
        consumer->awaitingLqTime.invalidate();
    }

    void gotoHighQuality(Consumer consumer)
    {
        const auto reader = consumer->display->getArchiveReader();
        if (reader->getQuality() != MEDIA_Quality_High)
        {
            trace(lit("Switching Low->High, %1 ms since last automatic change")
                .arg(lastAutoSwitchTimer.elapsed()));

            // Last actual change time (for performance warning only).
            lastModeChangeTimer.restart();
        }

        reader->setQuality(MEDIA_Quality_High, true);
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
                    && info.display->getArchiveReader()->getQuality() == MEDIA_Quality_Low
                    && info.lqReason == LqReason::performance;
            });
    }

    void optimizeConsumerQuality(Consumer consumer)
    {
        NX_ASSERT(consumer->mode == RadassMode::Auto);
        // Do not handle recently added items, some start animation can be in progress.
        if (!consumer->initialTime.hasExpired(kRecentlyAddedIntervalMs))
            return;

        const auto display = consumer->display;

        const auto reader = display->getArchiveReader();
        if (isForcedHqDisplay(display))
        {
            if (reader->getQuality() != MEDIA_Quality_High)
                trace("Forced switch to HQ", consumer);
            gotoHighQuality(consumer);
            return;
        }

        // Switch HQ->LQ if visual item size is small.
        if (reader->getQuality() == MEDIA_Quality_High
            && isSmallItem(display)
            && !reader->isMediaPaused())
        {
            // Run timer for the small items.
            if (!consumer->itemIsSmallInHq.isValid())
                consumer->itemIsSmallInHq.restart();

            if (consumer->itemIsSmallInHq.hasExpired(kLowerSmallItemQualityIntervalMs))
            {
                trace("Consumer was small in HQ for 1 second", consumer);
                gotoLowQuality(consumer, LqReason::smallItem);
                addHqTry();
            }
        }
        else
        {
            // Item is not small or not in HQ or not playing anymore.
            consumer->itemIsSmallInHq.invalidate();
        }

        // switch LQ->HQ for LIVE here.
        if (display->isRealTimeSource()
            // LQ live stream.
            && reader->getQuality() == MEDIA_Quality_Low
            // There are no a lot of packets in the queue (it is possible if CPU slow).
            && display->queueSize() <= (display->maxQueueSize() * 0.75)
            // Check if camera is not out of the screen
            && !reader->isPaused()
            // Check if camera is not paused
            && !reader->isMediaPaused()
            // No recently slow report by any camera.
            && lastSystemRtspDrop.hasExpired(kQualitySwitchIntervalMs)
            // Is big enough item for HQ.
            && isBigItem(display))
        {
            streamBackToNormal(consumer);
        }

        if (consumer->awaitingLqTime.isValid()
            && consumer->awaitingLqTime.hasExpired(kQualitySwitchIntervalMs))
        {
            trace("Consumer had slow stream for 5 seconds", consumer);
            gotoLowQuality(consumer, LqReason::performance);
        }
    }

    void setupNewConsumer(Consumer consumer)
    {
        if (consumer->mode != RadassMode::Auto)
            return;

        // Allow LQ->HQ switching.
        autoHqTransitionAllowed = true;

        if (isForcedHqDisplay(consumer->display))
        {
            trace("Setup new consumer: forced HQ", consumer);
            gotoHighQuality(consumer);
            return;
        }

        // If there are a lot of cameras on the layout, move all cameras to low quality if possible.
        if (consumers.size() >= kMaximumConsumersCount)
        {
            trace("Setup new consumer: too many cameras on the scene, going to LQ", consumer);
            gotoLowQuality(consumer, LqReason::tooManyItems);
            return;
        }

        // If there are at least one slow camera, make newly added camera low quality.
        const auto slow = slowConsumer();
        if (isValid(slow))
        {
            trace("Setup new consumer: there is slow consumer, going to LQ", consumer);
            gotoLowQuality(consumer, slow->lqReason);
        }
    }

    void onSlowStream(Consumer consumer)
    {
        trace("Slow stream detected.", consumer);
        lastSystemRtspDrop.restart();

        // Skip unsupported cameras and cameras with manual stream control.
        if (consumer->mode != RadassMode::Auto)
            return;

        const auto display = consumer->display;
        const auto reader = display->getArchiveReader();

        if (isForcedHqDisplay(display))
        {
            trace("Consumer is in forced HQ, leaving as is.", consumer);
            return;
        }

        // Check if reader already at LQ.
        if (reader->getQuality() == MEDIA_Quality_Low)
            return;

        // If item is in high speed, switch it to Lq immediately without triggering timer. Speed is
        // saved to an external variable to avoid race condition @rvasilenko.
        const auto speed = display->getSpeed();
        if (isFastForwardOrRevMode(speed))
        {
            trace("Consumer is in FF/Rew, swithing to LQ immediately.", consumer);
            gotoLowQuality(consumer, LqReason::performanceInFf, speed);
            return;
        }

        // Do not go to LQ if recently switch occurred.
        if (!lastAutoSwitchTimer.hasExpired(kQualitySwitchIntervalMs))
        {
            trace("Quality switch was less than 5 seconds ago, leaving as is.", consumer);
            consumer->awaitingLqTime.restart();
            return;
        }

        auto smallestConsumer = findConsumer(
            FindMethod::Smallest,
            MEDIA_Quality_High,
            itemQualityCanBeLowered);
        NX_ASSERT(isValid(smallestConsumer), "At least one camera must be found");
        if (!isValid(smallestConsumer))
        {
            trace("Smallest HQ camera was not found, using source instead.", consumer);
            smallestConsumer = consumer;
        }
        trace("Finding smallest HQ camera and switching it to LQ.", smallestConsumer);
        gotoLowQuality(smallestConsumer, LqReason::performance);
        lastAutoSwitchTimer.restart();
    }

    void streamBackToNormal(Consumer consumer)
    {
        // Camera do not have slow stream anymore.
        consumer->awaitingLqTime.invalidate();

        // Skip unsupported cameras and cameras with manual stream control.
        if (consumer->mode != RadassMode::Auto)
            return;

        QnCamDisplay* display = consumer->display;

        // Do not try HQ for small items.
        if (isSmallOrMediumItem(display))
        {
            trace("Consumer is not big enough", consumer);
            return;
        }

        if (consumer->lqReason == LqReason::performanceInFf)
        {
            const float oldSpeed = consumer->toLqSpeed;
            const float currentSpeed = display->getSpeed();
            if (!isFastForwardOrRevMode(currentSpeed)
                || qAbs(currentSpeed) < qAbs(oldSpeed))
            {
                // If item leave high speed mode change it back to HQ.
                trace("Consumer left FF mode or slowed, switching to HQ", consumer);
                gotoHighQuality(consumer);
                return;
            }
        }

        if (isFastForwardOrRevMode(display))
        {
            trace("Consumer still in FF mode, left in LQ", consumer);
            return;
        }

        // Auto-Hq transition is not blocked
        if (!autoHqTransitionAllowed)
        {
            trace("Auto-Hq transition is blocked", consumer);
            return;
        }


        // If item go to LQ because of small, return to HQ without delay.
        if (consumer->lqReason == LqReason::smallItem)
        {
            trace("Consumer was small, now big enough, go HQ", consumer);
            gotoHighQuality(consumer);
            return;
        }

        // Try one more LQ->HQ switch.

        // Recently LQ->HQ or HQ->LQ
        if (!lastAutoSwitchTimer.hasExpired(kQualitySwitchIntervalMs))
        {
            trace("Quality switch was less than 5 seconds ago, leaving as is.", consumer);
            return;
        }

        // Recently slow report received (not all reports affect d->lastAutoSwitchTimer).
        if (lastSystemRtspDrop.isValid() && !lastSystemRtspDrop.hasExpired(kQualitySwitchIntervalMs))
        {
            trace("RTSP drop was less than 5 seconds ago, leaving as is.", consumer);
            return;
        }

        // Do not go to HQ if some display perform opening.
        if (existsBufferingDisplay())
        {
            trace("Some item is opening right now, leaving as is.", consumer);
            return;
        }

        trace("Switching to hi quality", consumer);
        gotoHighQuality(consumer);
        lastAutoSwitchTimer.restart();
    }

    // Try LQ->HQ once more for all cameras.
    void addHqTry()
    {
        trace("Try LQ->HQ once more for all cameras");
        autoHqTransitionAllowed = true;
    }

    // Rearrange items quality: put small items to LQ state, large to HQ.
    void optimizeItemsQualityBySize()
    {
        // Do not optimize quality if recently switch occurred.
        if (!lastAutoSwitchTimer.hasExpired(kQualitySwitchIntervalMs))
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
            trace("Swap items quality");
            trace("Smaller item goes to LQ", smallConsumer);
            gotoLowQuality(smallConsumer, largeConsumer->lqReason);
            trace("Bigger item goes to HQ", largeConsumer);
            gotoHighQuality(largeConsumer);
            lastAutoSwitchTimer.restart();
        }
    }

    void reinitializeConsumersByCamera(const QnVirtualCameraResourcePtr& camera)
    {
        for (auto consumer = consumers.begin(); consumer != consumers.end(); ++consumer)
        {
            if (getCamera(consumer->display) != camera)
                continue;

            const bool isSupported = isRadassSupported(camera);
            const bool wasSupported = (consumer->mode != RadassMode::Custom);
            if (isSupported == wasSupported)
                continue;

            if (isSupported)
            {
                consumer->mode = RadassMode::Auto;
                setupNewConsumer(consumer);
            }
            else
            {
                consumer->mode = RadassMode::Custom;
            }
        }
    }

};

RadassController::RadassController(QObject* parent):
    base_type(parent),
    d(new Private())
{
    auto timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, &RadassController::onTimer);
    timer->start(kTimerIntervalMs);
}

RadassController::~RadassController()
{
}

void RadassController::onSlowStream(QnArchiveStreamReader* reader)
{
    QnMutexLocker lock(&d->mutex);

    auto consumer = d->findByReader(reader);
    if (!d->isValid(consumer))
        return;

    d->onSlowStream(consumer);
}

void RadassController::streamBackToNormal(QnArchiveStreamReader* reader)
{
    QnMutexLocker lock(&d->mutex);

    if (reader->getQuality() == MEDIA_Quality_High)
        return; // reader already at HQ

    auto consumer = d->findByReader(reader);
    if (!d->isValid(consumer))
        return;

    d->trace("Consumer stream back to normal.", consumer);
    d->streamBackToNormal(consumer);
}

void RadassController::onTimer()
{
    QnMutexLocker lock(&d->mutex);

    if (++d->timerTicks >= kAdditionalHQRetryCounter)
    {
        // Sometimes allow addition LQ->HQ switch.
        // Check if there were no slow stream reports lately.
        if (d->lastSystemRtspDrop.hasExpired(kQualitySwitchIntervalMs))
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
                    d->gotoLowQuality(consumer, LqReason::none);
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

    if (d->lastModeChangeTimer.hasExpired(kPerformanceLossCheckIntervalMs))
    {
        // Recently slow report received or there is a slow consumer.
        const auto performanceLoss =
            (d->lastSystemRtspDrop.isValid()
            && !d->lastSystemRtspDrop.hasExpired(kPerformanceLossCheckIntervalMs))
            || d->isValid(d->slowConsumer());

        const auto hasHq = std::any_of(d->consumers.cbegin(), d->consumers.cend(),
            [](const ConsumerInfo& info) { return info.mode == RadassMode::High; });

        if (hasHq && performanceLoss && !d->existsBufferingDisplay())
            emit performanceCanBeImproved();

        d->lastModeChangeTimer.restart();
    }
}

int RadassController::consumerCount() const
{
    QnMutexLocker lock(&d->mutex);
    return (int) d->consumers.size();
}

void RadassController::registerConsumer(QnCamDisplay* display)
{
    QnMutexLocker lock(&d->mutex);

    // Ignore cameras without readers. How can this be?
    auto reader = display->getArchiveReader();
    NX_ASSERT(reader);
    if (!reader)
        return;

    d->consumers.push_back(ConsumerInfo(display));
    auto consumer = d->consumers.end() - 1;
    NX_ASSERT(consumer->display == display);

    d->setupNewConsumer(consumer);
    d->lastModeChangeTimer.restart();

    // Listen to camera changes to make sure second stream will start working as soon as possible.
    if (const auto camera = getCamera(display))
    {
        if (d->cameras.insert(camera))
        {
            auto updateHasDualStreaming =
                [this, camera] { d->reinitializeConsumersByCamera(camera); };

            connect(camera, &QnResource::propertyChanged, this,
                [this, updateHasDualStreaming]
                (const QnResourcePtr& /*resource*/, const QString& propertyName)
                {
                    if (propertyName == ResourcePropertyKey::kMediaCapabilities
                        || propertyName == ResourcePropertyKey::kHasDualStreaming)
                    {
                        updateHasDualStreaming();
                    }
                });

            connect(camera, &QnSecurityCamResource::disableDualStreamingChanged, this,
                updateHasDualStreaming);
        }

    }

}

void RadassController::unregisterConsumer(QnCamDisplay* display)
{
    QnMutexLocker lock(&d->mutex);

    auto consumer = d->findByDisplay(display);
    if (!d->isValid(consumer))
        return;

    d->consumers.erase(consumer);
    d->addHqTry();
    d->lastModeChangeTimer.restart();

    if (const auto camera = getCamera(display))
    {
        if (d->cameras.remove(camera))
            camera->disconnect(this);
    }
}

RadassMode RadassController::mode(QnCamDisplay* display) const
{
    QnMutexLocker lock(&d->mutex);
    auto consumer = d->findByDisplay(display);
    NX_ASSERT(d->isValid(consumer));
    if (!d->isValid(consumer))
        return RadassMode::Auto;

    return consumer->mode;
}

void RadassController::setMode(QnCamDisplay* display, RadassMode mode)
{
    QnMutexLocker lock(&d->mutex);

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
            d->gotoLowQuality(consumer, LqReason::none);
            break;
        default:
            NX_ASSERT(false, "Should never get here");
            break;
    }
    d->lastModeChangeTimer.restart();
}

} // namespace nx::vms::client::desktop
