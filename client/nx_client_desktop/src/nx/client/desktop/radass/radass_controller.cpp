#include "radass_controller.h"

#include <QtCore/QTimer>
#include <QtCore/QElapsedTimer>

#include <core/resource/camera_resource.h>

#include <nx/client/desktop/radass/radass_types.h>

#include <camera/cam_display.h>

#include <nx/streaming/archive_stream_reader.h>
#include <nx/streaming/media_data_packet.h>

#include <nx/utils/log/assert.h>
#include <nx/utils/log/log.h>
#include <nx/utils/thread/mutex.h>

namespace {

// Time to ignore recently added cameras.
static constexpr int kRecentlyAddedIntervalMs = 1000;

// Delay between quality switching attempts.
static constexpr int kQualitySwitchIntervalMs = 1000 * 5;

// Some item stuck after HQ switching. Do not switch to HQ after this number of retries.
static constexpr int kHighQualityRetryCounter = 1;

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

bool isForcedHqDisplay(QnCamDisplay* display)
{
    return display->isFullScreen() || display->isZoomWindow() || display->isFisheyeEnabled();
}

// Omit cameras without dual streaming.
bool isSupportedDisplay(QnCamDisplay* display)
{
    if (!display || !display->getArchiveReader())
        return false;

    auto cam = display->getArchiveReader()->getResource().dynamicCast<QnSecurityCamResource>();
    return cam && cam->hasDualStreaming2();
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

bool isFFSpeed(QnCamDisplay* display)
{
    const auto speed = display->getSpeed();
    return speed > 1 + kSpeedValueEpsilon || speed < 0;
}

enum class LqReason
{
    None,
    Small,
    Network,
    CPU,
    FF
};

LqReason slowLqReason(QnCamDisplay* display)
{
    return display->queueSize() < kSlowNetworkFrameLimit ? LqReason::Network : LqReason::CPU;
}

enum class FindMethod
{
    Biggest,
    Smallest
};

struct ConsumerInfo
{
    using Mode = nx::client::desktop::RadassMode;

    QnCamDisplay* display;

    float toLQSpeed = 0.0;
    LqReason lqReason = LqReason::None;
    QElapsedTimer initialTime;
    QElapsedTimer awaitingLqTime;
    QElapsedTimer lqTime;

    // Item will go to LQ if it is small for this period of time already.
    QElapsedTimer itemIsSmall;

    // Custom mode means unsupported consumer.
    Mode mode = Mode::Auto;

    // Required for std::vector resize.
    ConsumerInfo() = default;

    ConsumerInfo(QnCamDisplay* display):
        display(display)
    {
        initialTime.start();
        if (!isSupportedDisplay(display))
            mode = Mode::Custom;
    }
};

} // namespace

namespace nx {
namespace client {
namespace desktop {

struct RadassController::Private
{
    mutable QnMutex mutex;
    QElapsedTimer lastSwitchTimer; //< Latest HQ->LQ or LQ->HQ switch.
    int hiQualityRetryCounter = 0;
    int timerTicks = 0;    //< onTimer ticks count
    QElapsedTimer lastLqTime; //< Latest HQ->LQ switch time.

    std::vector<ConsumerInfo> consumers;
    using Consumer = decltype(consumers)::iterator;

    Private():
        mutex(QnMutex::Recursive)
    {
        lastSwitchTimer.start();
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
        const bool findHQ = findQuality == MEDIA_Quality_High;
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

            QSize size = display->getMaxScreenSize();
            qint64 screenSquare = size.width() * size.height();

            // Using pixels per second for more granular sorting.
            QSize res = display->getVideoSize();
            int pps = res.width() * res.height() * display->getAvarageFps();

            // Put display with max pps to the beginning, so if slow stream do HQ->LQ for max pps
            // display (between displays with same size).
            pps = INT_MAX - pps;

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

            auto consumer = itr.value();
            auto display = consumer->display;
            QnArchiveStreamReader* reader = display->getArchiveReader();
            bool isReaderHQ = reader->getQuality() == MEDIA_Quality_High;
            if (isReaderHQ == findHQ)
            {
                if (displaySize)
                    *displaySize = itr.key() >> 32;
                return consumer;
            }
        }

        return consumers.end();
    }

    void gotoLowQuality(Consumer consumer, LqReason reason, double speed = INT_MAX)
    {
        auto oldReason = consumer->lqReason;
        if ((oldReason == LqReason::Network || oldReason == LqReason::CPU)
            && (reason == LqReason::Network || reason == LqReason::CPU))
        {
            hiQualityRetryCounter++; //< Item goes to LQ again because not enough resources.
        }

        consumer->display->getArchiveReader()->setQuality(MEDIA_Quality_Low, true);
        consumer->lqReason = reason;
        // Get speed for FF reason as external variable to prevent race condition.
        consumer->toLQSpeed = speed != INT_MAX ? speed : consumer->display->getSpeed();
        consumer->awaitingLqTime.invalidate();
    }

    void gotoHighQuality(Consumer consumer)
    {
        consumer->display->getArchiveReader()->setQuality(MEDIA_Quality_High, true);
    }

    bool existsBufferingDisplay() const
    {
        return std::any_of(consumers.cbegin(), consumers.cend(),
            [](const ConsumerInfo& info) { return info.display->isBuffering(); });
    }

    void optimizeConsumerQuality(Consumer consumer)
    {
        NX_ASSERT(consumer->mode == RadassMode::Auto);

        // Do not handle recently added items, some start animation can be in progress.
        if (!consumer->initialTime.hasExpired(kRecentlyAddedIntervalMs))
            return;

        QnCamDisplay* display = consumer->display;

        // Switch HQ->LQ if visual item size is small.
        QnArchiveStreamReader* reader = display->getArchiveReader();

        if (isForcedHqDisplay(display) && !isFFSpeed(display))
        {
            gotoHighQuality(consumer);
        }
        else if (consumer->awaitingLqTime.isValid()
            && consumer->awaitingLqTime.hasExpired(kQualitySwitchIntervalMs))
        {
            gotoLowQuality(consumer, slowLqReason(display));
        }
        else
        {
            if (reader->getQuality() == MEDIA_Quality_High
                && isSmallItem(display)
                && !reader->isMediaPaused())
            {
                // Run timer forthe small items.
                if (!consumer->itemIsSmall.isValid())
                    consumer->itemIsSmall.restart();

                if (consumer->itemIsSmall.hasExpired(kLowerSmallItemQualityIntervalMs))
                {
                    gotoLowQuality(consumer, LqReason::Small);
                    addHQTry();
                }
            }
            else
            {
                // Item is not small anymore.
                consumer->itemIsSmall.invalidate();
            }
        }

        // switch LQ->HQ for LIVE here.
        if (display->isRealTimeSource()
            // LQ live stream.
            && reader->getQuality() == MEDIA_Quality_Low
            // No drop report several last seconds.
            && consumer->lqTime.hasExpired(kQualitySwitchIntervalMs)
            // There are no a lot of packets in the queue (it is possible if CPU slow).
            && display->queueSize() <= display->maxQueueSize() / 2
            // No recently slow report by any camera.
            && !lastSwitchTimer.hasExpired(kQualitySwitchIntervalMs)
            // Is big enough item for HQ.
            && isBigItem(display))
        {
            streamBackToNormal(consumer);
        }
    }

    void setupNewConsumer(Consumer consumer)
    {
        if (consumer->mode != RadassMode::Auto)
            return;

        if (isForcedHqDisplay(consumer->display))
        {
            gotoHighQuality(consumer);
            return;
        }

        if (consumers.size() >= kMaximumConsumersCount)
        {
            gotoLowQuality(consumer, LqReason::Small);
            return;
        }

        // If there are at least one slow camera, make newly added camera low quality.
        for (auto otherConsumer = consumers.cbegin();
            otherConsumer != consumers.cend();
            ++otherConsumer)
        {
            if (otherConsumer->mode == RadassMode::Auto
                && otherConsumer->display->getArchiveReader()->getQuality() == MEDIA_Quality_Low
                && (otherConsumer->lqReason == LqReason::CPU
                    || otherConsumer->lqReason == LqReason::Network))
            {
                gotoLowQuality(consumer, otherConsumer->lqReason);
                break;
            }
        }

    }

    void onSlowStream(Consumer consumer)
    {
        // Skip unsupported cameras and cameras with manual stream control.
        if (consumer->mode != RadassMode::Auto)
            return;

        lastLqTime.restart();
        consumer->lqTime.restart();

        auto display = consumer->display;
        auto reader = display->getArchiveReader();

        if (isFFSpeed(display))
        {
            // For high speed mode change same item to LQ (do not try to find least item).
            gotoLowQuality(consumer, LqReason::FF, display->getSpeed());
            return;
        }

        if (isForcedHqDisplay(display))
            return;

        // Check if reader already at LQ.
        if (reader->getQuality() == MEDIA_Quality_Low)
            return;

        // Do not go to LQ if recently switch occurred.
        if (!lastSwitchTimer.hasExpired(kQualitySwitchIntervalMs))
        {
            consumer->awaitingLqTime.restart();
            return;
        }

        gotoLowQuality(consumer, slowLqReason(consumer->display));
        lastSwitchTimer.restart();
    }

    void streamBackToNormal(Consumer consumer)
    {
        // Skip unsupported cameras and cameras with manual stream control.
        if (consumer->mode != RadassMode::Auto)
            return;

        consumer->awaitingLqTime.invalidate();

        QnCamDisplay* display = consumer->display;
        if (qAbs(display->getSpeed()) < consumer->toLQSpeed
            || (consumer->toLQSpeed < 0 && display->getSpeed() > 0))
        {
            // If item leave high speed mode change same item to HQ (do not try to find biggest item)
            gotoHighQuality(consumer);
            return;
        }

        // Do not return to HQ in FF mode because of retry counter is not increased for FF.
        if (isFFSpeed(display))
            return;

        // Some item stuck after HQ switching. Do not switch to HQ any more.
        if (hiQualityRetryCounter >= kHighQualityRetryCounter)
            return;

        // Do not try HQ for small items.
        if (isSmallOrMediumItem(display))
            return;

        // If item go to LQ because of small, return to HQ without delay.
        if (consumer->lqReason == LqReason::Small)
        {
            gotoHighQuality(consumer);
            return;
        }

        // Try one more LQ->HQ switch.

        // Recently LQ->HQ or HQ->LQ
        if (!lastSwitchTimer.hasExpired(kQualitySwitchIntervalMs))
            return;

        // Recently slow report received (not all reports affect d->lastSwitchTimer).
        if (lastLqTime.isValid() && !lastLqTime.hasExpired(kQualitySwitchIntervalMs))
            return;

        // Do not go to HQ if some display perform opening.
        if (existsBufferingDisplay())
            return;

        gotoHighQuality(consumer);
        lastSwitchTimer.restart();
    }

    // Try LQ->HQ once more.
    void addHQTry()
    {
        hiQualityRetryCounter = qMin(hiQualityRetryCounter, kHighQualityRetryCounter);
        hiQualityRetryCounter = qMax(0, hiQualityRetryCounter - 1);
    }

    // Rearrange items quality: put small items to LQ state, large to HQ.
    void optimizeItemsQualityBySize()
    {
        // Do not optimize quality if recently switch occurred.
        if (!lastSwitchTimer.hasExpired(kQualitySwitchIntervalMs))
            return;

        // Do not rearrange items if any item is in FF/REW mode right now.
        if (std::any_of(consumers.cbegin(), consumers.cend(),
            [](const ConsumerInfo& consumer) { return isFFSpeed(consumer.display); }))
        {
            return;
        }

        // Find the biggest camera with auto control and low quality.
        int largeSize = 0;
        auto largeConsumer = findConsumer(FindMethod::Biggest,
            MEDIA_Quality_Low,
            itemQualityCanBeRaised,
            &largeSize);
        if (!isValid(largeConsumer))
            return;

        // Find the smallest camera with auto control and hi quality.
        int smallSize = 0;
        auto smallConsumer = findConsumer(FindMethod::Smallest,
            MEDIA_Quality_High,
            itemQualityCanBeLowered,
            &smallSize);
        if (!isValid(smallConsumer))
            return;

        if (largeSize >= smallSize * 2)
        {
            // swap items quality
            gotoLowQuality(smallConsumer, largeConsumer->lqReason);
            gotoHighQuality(largeConsumer);
            lastSwitchTimer.restart();
        }
    }

};

RadassController::RadassController():
    d(new Private())
{
    auto timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, &RadassController::onTimer);
    timer->start(kTimerIntervalMs);
}

RadassController::~RadassController()
{
    delete d;
    d = nullptr;
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

    d->streamBackToNormal(consumer);
}

void RadassController::onTimer()
{
    QnMutexLocker lock(&d->mutex);

    if (++d->timerTicks >= kAdditionalHQRetryCounter)
    {
        // Sometimes allow addition LQ->HQ switch.
        // Check if there were no slow stream reports lately.
        if (d->lastLqTime.hasExpired(kQualitySwitchIntervalMs))
        {
            d->addHQTry();
            d->timerTicks = 0;
        }
    }

    for (auto consumer = d->consumers.begin(); consumer != d->consumers.end(); ++consumer)
    {
        switch (consumer->mode)
        {
            case RadassMode::High:
                d->gotoHighQuality(consumer);
                break;
            case RadassMode::Low:
                if (!isForcedHqDisplay(consumer->display))
                    d->gotoLowQuality(consumer, LqReason::None);
                break;
            case RadassMode::Custom:
                // Skip unsupported cameras.
                break;
            case RadassMode::Auto:
                d->optimizeConsumerQuality(consumer);
                break;
            default:
                break;
        }
    }

    d->optimizeItemsQualityBySize();
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

    // If there are a lot of cameras on the layout, move all cameras to low quality if possible.
    if (consumerCount() >= kMaximumConsumersCount)
    {
        for (auto consumer = d->consumers.begin(); consumer != d->consumers.end(); ++consumer)
        {
            if (consumer->mode == RadassMode::Auto && !isForcedHqDisplay(consumer->display))
                d->gotoLowQuality(consumer, LqReason::Small);
        }
    }
    else
    {
        d->setupNewConsumer(consumer);
    }
}

void RadassController::unregisterConsumer(QnCamDisplay* display)
{
    QnMutexLocker lock(&d->mutex);

    auto consumer = d->findByDisplay(display);
    if (!d->isValid(consumer))
        return;

    d->consumers.erase(consumer);
    d->addHQTry();
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
            // Allow LQ->HQ switching.
            d->hiQualityRetryCounter = 0;
            d->setupNewConsumer(consumer);
            break;
        case RadassMode::High:
            d->gotoHighQuality(consumer);
            break;
        case RadassMode::Low:
            // What if full screen or zoom window?
            d->gotoLowQuality(consumer, LqReason::None);
            break;
        default:
            NX_ASSERT(false, "Should never get here");
            break;
    }
}


} // namespace desktop
} // namespace client
} // namespace nx
