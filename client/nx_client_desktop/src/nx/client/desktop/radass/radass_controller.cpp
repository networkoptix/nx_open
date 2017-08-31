#include "radass_controller.h"

#include <QtCore/QTimer>
#include <QtCore/QElapsedTimer>

#include <core/resource/camera_resource.h>

#include <nx/client/desktop/radass/radass_types.h>

#include <camera/cam_display.h>

#include <nx/streaming/archive_stream_reader.h>
#include <nx/streaming/media_data_packet.h>

#include <utils/common/synctime.h>

#include <nx/utils/log/assert.h>
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

bool isForcedHqDisplay(QnCamDisplay* display)
{
    return display->isFullScreen() || display->isZoomWindow() || display->isFisheyeEnabled();
}

// TODO: #GDM Do not see any status checks here.
// Omit cameras without dual streaming, offline and non-authorized cameras.
bool isSupportedDisplay(QnCamDisplay* display)
{
    if (!display || !display->getArchiveReader())
        return false;

    auto cam = display->getArchiveReader()->getResource().dynamicCast<QnSecurityCamResource>();
    return cam && cam->hasDualStreaming();
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

bool itemCanBeOptimized(QnCamDisplay* display)
{
    // TODO: #GDM shouldn't we check camera control mode here?
    return !isSmallItem(display) && !isForcedHqDisplay(display);
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

    qint64 lqTime = 0;
    float toLQSpeed = 0.0;
    LqReason lqReason = LqReason::None;
    QElapsedTimer initialTime;
    QElapsedTimer awaitingLQTime;
    int smallSizeCnt = 0;

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
    QElapsedTimer lastSwitchTimer; //< Latest HQ->LQ or LQ->HQ switch
    int hiQualityRetryCounter = 0;
    int timerTicks = 0;    // onTimer ticks count
    qint64 lastLqTime = 0; // latest HQ->LQ switch time

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
    Consumer findDisplay(FindMethod method,
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
            bool isReaderHQ = reader->getQuality() == MEDIA_Quality_High
                || reader->getQuality() == MEDIA_Quality_ForceHigh;

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
        consumer->awaitingLQTime.invalidate();
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

};

RadassController::RadassController():
    d(new Private())
{
    auto timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, &RadassController::onTimer);
    timer->start(kTimerIntervalMs);
}

void RadassController::onSlowStream(QnArchiveStreamReader* reader)
{
    QnMutexLocker lock(&d->mutex);

    auto consumer = d->findByReader(reader);
    if (!d->isValid(consumer))
        return;

    // TODO: #GDM why check current camera while we are swithcing another one???
    // Skip unsupported cameras and cameras with manual stream control.
    if (consumer->mode != RadassMode::Auto)
        return;

    d->lastLqTime = consumer->lqTime = qnSyncTime->currentMSecsSinceEpoch();

    QnCamDisplay* display = consumer->display;
    if (isFFSpeed(display))
    {
        // For high speed mode change same item to LQ (do not try to find least item).
        d->gotoLowQuality(consumer, LqReason::FF, display->getSpeed());
        return;
    }

    if (isForcedHqDisplay(display))
        return;

    if (reader->getQuality() == MEDIA_Quality_Low)
        return; // reader already at LQ

    // Do not go to LQ if recently switch occurred.
    if (!d->lastSwitchTimer.hasExpired(kQualitySwitchIntervalMs))
    {
        consumer->awaitingLQTime.restart();
        return;
    }

    // switch to LQ min item
    auto minConsumer = d->findDisplay(FindMethod::Smallest, MEDIA_Quality_High);
    if (d->isValid(minConsumer))
    {
        // TODO: #GDM What's going on here??? Why moving us but info from the lowest display??? WTF?
        d->gotoLowQuality(consumer, slowLqReason(consumer->display));
        d->lastSwitchTimer.restart();
    }
}

void RadassController::streamBackToNormal(QnArchiveStreamReader* reader)
{
    QnMutexLocker lock(&d->mutex);

    if (reader->getQuality() == MEDIA_Quality_High || reader->getQuality() == MEDIA_Quality_ForceHigh)
        return; // reader already at HQ

    auto consumer = d->findByReader(reader);
    if (!d->isValid(consumer))
        return;

    // Skip unsupported cameras and cameras with manual stream control.
    if (consumer->mode != RadassMode::Auto)
        return;

    consumer->awaitingLQTime.invalidate();

    QnCamDisplay* display = consumer->display;
    if (qAbs(display->getSpeed()) < consumer->toLQSpeed
        || (consumer->toLQSpeed < 0 && display->getSpeed() > 0))
    {
        // If item leave high speed mode change same item to HQ (do not try to find biggest item)
        d->gotoHighQuality(consumer);
        return;
    }

    // Do not return to HQ in FF mode because of retry counter is not increased for FF.
    if (isFFSpeed(display))
        return;

    // Some item stuck after HQ switching. Do not switch to HQ any more.
    if (d->hiQualityRetryCounter >= kHighQualityRetryCounter)
        return;

    // Do not try HQ for small items.
    if (isSmallOrMediumItem(display)) //TODO: #GDM why not checking 'isSmall' here?
        return;

    // If item go to LQ because of small, return to HQ without delay.
    if (consumer->lqReason == LqReason::Small)
    {
        d->gotoHighQuality(consumer);
        return;
    }

    // Try one more LQ->HQ switch.

    // Recently LQ->HQ or HQ->LQ
    if (!d->lastSwitchTimer.hasExpired(kQualitySwitchIntervalMs))
        return;

    // Recently slow report received (not all reports affect d->lastSwitchTimer)
    if (d->lastLqTime + kQualitySwitchIntervalMs > qnSyncTime->currentMSecsSinceEpoch())
        return;

    if (d->existsBufferingDisplay())
        return; // do not go to HQ if some display perform opening...

    auto maxConsumer = d->findDisplay(FindMethod::Biggest, MEDIA_Quality_Low, isBigItem);
    if (d->isValid(maxConsumer))
    {
        d->gotoHighQuality(maxConsumer);
        d->lastSwitchTimer.restart();
    }
}

void RadassController::onTimer()
{
    QnMutexLocker lock(&d->mutex);

    // TODO: #GDM fixme!
//     if (m_mode != RadassMode::Auto)
//     {
//         for (auto itr = m_consumersInfo.begin(); itr != m_consumersInfo.end(); ++itr)
//         {
//             QnCamDisplay* display = itr.key();
//             QnArchiveStreamReader* reader = display->getArchiveReader();
//             if (!display->isFullScreen()) // TODO: #GDM what about fisheye and others? Why are we doing this on every timer tick?
//                 reader->setQuality(m_mode == RadassMode::Low ? MEDIA_Quality_Low : MEDIA_Quality_High, true);
//         }
//         return;
//     }

    if (++d->timerTicks >= kAdditionalHQRetryCounter)
    {
        // sometimes allow addition LQ->HQ switch
        bool slowStreamExists = false;
        for (auto info: d->consumers)
        {
            if (qnSyncTime->currentMSecsSinceEpoch() < info.lqTime + kQualitySwitchIntervalMs)
                slowStreamExists = true;
        }
        if (!slowStreamExists)
        {
            addHQTry();
            d->timerTicks = 0;
        }
    }

    for (auto consumer = d->consumers.begin(); consumer != d->consumers.end(); ++consumer)
    {
        // Do not handle recently added items, some start animation can be in progress.
        if (!consumer->initialTime.hasExpired(kRecentlyAddedIntervalMs))
            continue;

        QnCamDisplay* display = consumer->display;

        // TODO: #GDM check single itme mode here?
        if (!isSupportedDisplay(display))
            continue;

        // Switch HQ->LQ if visual item size is small.
        QnArchiveStreamReader* reader = display->getArchiveReader();

        if (isForcedHqDisplay(display) && !isFFSpeed(display))
        {
            d->gotoHighQuality(consumer);
        }
        else if (consumer->awaitingLQTime.isValid()
            && consumer->awaitingLQTime.hasExpired(kQualitySwitchIntervalMs))
        {
            d->gotoLowQuality(consumer, slowLqReason(display));
        }
        else
        {
            if (reader->getQuality() == MEDIA_Quality_High
                && isSmallItem(display)
                && !reader->isMediaPaused())
            {
                consumer->smallSizeCnt++;
                if (consumer->smallSizeCnt > 1)
                {
                    d->gotoLowQuality(consumer, LqReason::Small);
                    addHQTry();
                }
            }
            else
            {
                consumer->smallSizeCnt = 0;
            }
        }

        // switch LQ->HQ for LIVE here.
        if (display->isRealTimeSource()
            // LQ live stream.
            && reader->getQuality() == MEDIA_Quality_Low
            // No drop report several last seconds.
            && qnSyncTime->currentMSecsSinceEpoch() - consumer->lqTime > kQualitySwitchIntervalMs
            // There are no a lot of packets in the queue (it is possible if CPU slow).
            && display->queueSize() <= display->maxQueueSize() / 2
            // No recently slow report by any camera.
            && !d->lastSwitchTimer.hasExpired(kQualitySwitchIntervalMs)
            // Is big enough item for HQ.
            && isBigItem(display))
        {
            streamBackToNormal(reader);
        }
    }

    optimizeItemsQualityBySize();
}

void RadassController::optimizeItemsQualityBySize()
{
    // Do not optimize quality if recently switch occurred.
    if (!d->lastSwitchTimer.hasExpired(kQualitySwitchIntervalMs))
        return;

    for (auto consumer: d->consumers)
    {
        if (isFFSpeed(consumer.display))
            return; // do not rearrange items if FF/REW mode // TODO: #GDM Why return??? What if the item is in manual mode.

        // TODO: #GDM looks like we need to get a list of items here.
        if (consumer.mode != RadassMode::Auto)
            continue;
    }

    int largeSize = 0;
    int smallSize = 0;
    auto largeConsumer = d->findDisplay(FindMethod::Biggest,
        MEDIA_Quality_Low,
        itemCanBeOptimized,
        &largeSize);
    auto smallConsumer = d->findDisplay(FindMethod::Smallest,
        MEDIA_Quality_High,
        itemCanBeOptimized,
        &smallSize);

    if (d->isValid(largeConsumer) && d->isValid(smallConsumer) && largeSize >= smallSize * 2)
    {
        // swap items quality
        d->gotoLowQuality(smallConsumer, largeConsumer->lqReason);
        d->gotoHighQuality(largeConsumer);
        d->lastSwitchTimer.restart();
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
    QnArchiveStreamReader* reader = display->getArchiveReader();
    if (display->getArchiveReader())
    {
        d->consumers.push_back(ConsumerInfo(display));
        auto consumer = d->consumers.end() - 1;
        NX_ASSERT(consumer->display == display);

        if (isSupportedDisplay(display))
        {
            if (!isForcedHqDisplay(display))
            {
                if (consumerCount() >= kMaximumConsumersCount)
                {
                    d->gotoLowQuality(consumer, LqReason::Network);
                }
                else
                {
                    for (auto info: d->consumers)
                    {
                        if (info.display->getArchiveReader()->getQuality() == MEDIA_Quality_Low
                            && info.lqReason != LqReason::Small)
                        {
                            d->gotoLowQuality(consumer, info.lqReason);
                        }
                    }
                }
            }
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
    addHQTry();
}

void RadassController::addHQTry()
{
    d->hiQualityRetryCounter = qMin(d->hiQualityRetryCounter, kHighQualityRetryCounter);
    d->hiQualityRetryCounter = qMax(0, d->hiQualityRetryCounter - 1);
}

// void RadassController::setMode(RadassMode mode)
// {
//      QnMutexLocker lock(&d->mutex);
//
//      if (m_mode == mode)
//          return;
//
//      m_mode = mode;
//
//      if (m_mode == RadassMode::Auto)
//      {
//          d->hiQualityRetryCounter = 0; // allow LQ->HQ switching
//      }
//      else
//      {
//          for (auto itr = m_consumersInfo.begin(); itr != m_consumersInfo.end(); ++itr)
//          {
//              QnCamDisplay* display = itr.key();
//
//              if (!isSupportedDisplay(display))
//                  continue; // ommit cameras without dual streaming, offline and non-authorized cameras
//
//              QnArchiveStreamReader* reader = display->getArchiveReader();
//              reader->setQuality(m_mode == RadassMode::High ? MEDIA_Quality_High : MEDIA_Quality_Low, true);
//          }
//      }
// }
//
// RadassMode RadassController::getMode() const
// {
//     QnMutexLocker lock(&d->mutex);
//     return m_mode;
// }

} // namespace desktop
} // namespace client
} // namespace nx
