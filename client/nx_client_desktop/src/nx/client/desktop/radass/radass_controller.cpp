#include "radass_controller.h"

#include <nx/client/desktop/radass/radass_types.h>

#include "camera/cam_display.h"
#include "nx/streaming/archive_stream_reader.h"
#include "core/resource/camera_resource.h"

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

} // namespace

namespace nx {
namespace client {
namespace desktop {

struct RadassController::ConsumerInfo
{
    qint64 lqTime = 0;
    float toLQSpeed = 0.0;
    LQReason lqReason = Reason_None ;
    QElapsedTimer initialTime;
    QElapsedTimer awaitingLQTime;
    int smallSizeCnt = 0;

    ConsumerInfo()
    {
        initialTime.start();
    }
};

RadassController::RadassController():
    m_mutex(QnMutex::Recursive),
    m_mode(RadassMode::Auto)
{
    QObject::connect(&m_timer, &QTimer::timeout, this, &RadassController::onTimer);
    m_timer.start(kTimerIntervalMs);
    m_lastSwitchTimer.start();
    m_hiQualityRetryCounter = 0;
    m_timerTicks = 0;
    m_lastLqTime = 0;
}

QnCamDisplay* RadassController::getDisplayByReader(QnArchiveStreamReader* reader)
{
    for (auto itr = m_consumersInfo.begin(); itr != m_consumersInfo.end(); ++itr)
    {
        QnCamDisplay* display = itr.key();
        if (display->getArchiveReader() == reader)
            return display;
    }
    return 0;
}

bool RadassController::isSupportedDisplay(QnCamDisplay* display) const
{
    if (!display || !display->getArchiveReader())
        return false;

    auto cam = display->getArchiveReader()->getResource().dynamicCast<QnSecurityCamResource>();
    return cam && cam->hasDualStreaming();
}

QnCamDisplay* RadassController::findDisplay(FindMethod method, MediaQuality findQuality, SearchCondition cond, int* displaySize)
{
    bool findHQ = findQuality == MEDIA_Quality_High;
    if (displaySize)
        *displaySize = 0;
    QMultiMap<qint64, QnCamDisplay*> camDisplayByScreenSize;
    for (auto itr = m_consumersInfo.begin(); itr != m_consumersInfo.end(); ++itr)
    {
        QnCamDisplay* display = itr.key();
        if (!isSupportedDisplay(display))
            continue; // ommit cameras without dual streaming, offline and non-authorized cameras

        QSize size = display->getMaxScreenSize();
        QSize res = display->getVideoSize();
        qint64 screenSquare = size.width() * size.height();
        int pps = res.width()*res.height()*display->getAvarageFps(); // pps - pixels per second
        pps = INT_MAX - pps; // put display with max pps to the begin, so if slow stream do HQ->LQ for max pps disply (between displays with same size)
        camDisplayByScreenSize.insert((screenSquare << 32) + pps, display);
    }

    QMapIterator<qint64, QnCamDisplay*> itr(camDisplayByScreenSize);
    if (method == Find_Biggest)
        itr.toBack();

    while ((method == Find_Least && itr.hasNext()) || (method == Find_Biggest && itr.hasPrevious()))
    {
        if (method == Find_Least)
            itr.next();
        else
            itr.previous();
        QnCamDisplay* display = itr.value();
        QnArchiveStreamReader* reader = display->getArchiveReader();
        bool isReaderHQ = reader->getQuality() == MEDIA_Quality_High || reader->getQuality() == MEDIA_Quality_ForceHigh;
        if (isReaderHQ == findHQ && (!cond || (this->*cond)(display)))
        {
            if (displaySize)
                *displaySize = itr.key() >> 32;
            return display;
        }
    }
    return 0;
}

bool RadassController::isForcedHQDisplay(QnCamDisplay* display, QnArchiveStreamReader* /*reader*/) const
{
    return display->isFullScreen() || display->isZoomWindow() || display->isFisheyeEnabled();
}

void RadassController::onSlowStream(QnArchiveStreamReader* reader)
{
    QnMutexLocker lock(&m_mutex);

    if (m_mode != RadassMode::Auto)
        return;

    QnCamDisplay* display = getDisplayByReader(reader);
    if (!isSupportedDisplay(display))
        return;
    m_lastLqTime = m_consumersInfo[display].lqTime = qnSyncTime->currentMSecsSinceEpoch();


    double speed = display->getSpeed();
    if (isFFSpeed(speed))
    {
        // For high speed mode change same item to LQ (do not try to find least item).
        gotoLowQuality(display, Reson_FF, speed);
        return;
    }

    if (isForcedHQDisplay(display, reader))
        return;

    if (reader->getQuality() == MEDIA_Quality_Low)
        return; // reader already at LQ

    // Do not go to LQ if recently switch occurred.
    if (!m_lastSwitchTimer.hasExpired(kQualitySwitchIntervalMs))
    {
        m_consumersInfo[display].awaitingLQTime.restart();
        return;
    }

    // switch to LQ min item
    display = findDisplay(Find_Least, MEDIA_Quality_High);
    if (display)
    {
        gotoLowQuality(display, display->queueSize() < 3 ? Reason_Network : Reason_CPU);
        m_lastSwitchTimer.restart();
    }
}

bool RadassController::existstBufferingDisplay() const
{
    for (auto itr = m_consumersInfo.constBegin(); itr != m_consumersInfo.constEnd(); ++itr)
    {
        const QnCamDisplay* display = itr.key();
        if (display->isBuffering())
            return true;
    }
    return false;
}

void RadassController::streamBackToNormal(QnArchiveStreamReader* reader)
{
    QnMutexLocker lock(&m_mutex);

    if (m_mode != RadassMode::Auto)
        return;

    if (reader->getQuality() == MEDIA_Quality_High || reader->getQuality() == MEDIA_Quality_ForceHigh)
        return; // reader already at HQ

    QnCamDisplay* display = getDisplayByReader(reader);
    if (!isSupportedDisplay(display))
        return;

    m_consumersInfo[display].awaitingLQTime.invalidate();

    if (qAbs(display->getSpeed()) < m_consumersInfo[display].toLQSpeed
        || (m_consumersInfo[display].toLQSpeed < 0 && display->getSpeed() > 0))
    {
        // If item leave high speed mode change same item to HQ (do not try to find biggest item)
        reader->setQuality(MEDIA_Quality_High, true);
        return;
    }

    // Do not return to HQ in FF mode because of retry counter is not increased for FF.
    if (isFFSpeed(display))
        return;

    // Some item stuck after HQ switching. Do not switch to HQ any more.
    if (m_hiQualityRetryCounter >= kHighQualityRetryCounter)
        return;

    // Do not try HQ for small items.
    if (isSmallItem2(display))
        return;

    // If item go to LQ because of small, return to HQ without delay.
    if (m_consumersInfo[display].lqReason == Reason_Small)
    {
        reader->setQuality(MEDIA_Quality_High, true);
        return;
    }

    // Try one more LQ->HQ switch.

    // Recently LQ->HQ or HQ->LQ
    if (!m_lastSwitchTimer.hasExpired(kQualitySwitchIntervalMs))
        return;

    // Recently slow report received (not all reports affect m_lastSwitchTimer)
    if (m_lastLqTime + kQualitySwitchIntervalMs > qnSyncTime->currentMSecsSinceEpoch())
        return;

    if (existstBufferingDisplay())
        return; // do not go to HQ if some display perform opening...

    display = findDisplay(Find_Biggest, MEDIA_Quality_Low, &RadassController::isNotSmallItem2);
    if (display)
    {
        display->getArchiveReader()->setQuality(MEDIA_Quality_High, true);
        m_lastSwitchTimer.restart();
    }
}

bool RadassController::isSmallItem(QnCamDisplay* display)
{
    QSize sz = display->getMaxScreenSize();
    return sz.height() <= kLowQualityScreenSize.height();
}

bool RadassController::isSmallItem2(QnCamDisplay* display)
{
    QSize sz = display->getMaxScreenSize();
    return sz.height() <= kLowQualityScreenSize.height() * kLqToHqSizeThreshold;
}

bool RadassController::itemCanBeOptimized(QnCamDisplay* display)
{
    QnArchiveStreamReader* reader = display->getArchiveReader();
    return !isSmallItem(display) && !isForcedHQDisplay(display, reader);
}

bool RadassController::isNotSmallItem2(QnCamDisplay* display)
{
    return !isSmallItem2(display);
}

bool RadassController::isFFSpeed(QnCamDisplay* display) const
{
    return isFFSpeed(display->getSpeed());
}

bool RadassController::isFFSpeed(double speed) const
{
    return speed > 1 + kSpeedValueEpsilon || speed < 0;
}

void RadassController::onTimer()
{
    QnMutexLocker lock(&m_mutex);

    if (m_mode != RadassMode::Auto)
    {
        for (auto itr = m_consumersInfo.begin(); itr != m_consumersInfo.end(); ++itr)
        {
            QnCamDisplay* display = itr.key();
            QnArchiveStreamReader* reader = display->getArchiveReader();
            if (!display->isFullScreen())
                reader->setQuality(m_mode == RadassMode::Low ? MEDIA_Quality_Low : MEDIA_Quality_High, true);
        }
        return;
    }

    if (++m_timerTicks >= kAdditionalHQRetryCounter)
    {
        // sometimes allow addition LQ->HQ switch
        bool slowStreamExists = false;
        for (auto itr = m_consumersInfo.begin(); itr != m_consumersInfo.end(); ++itr)
        {
            if (qnSyncTime->currentMSecsSinceEpoch() < itr.value().lqTime + kQualitySwitchIntervalMs)
                slowStreamExists = true;
        }
        if (!slowStreamExists)
        {
            addHQTry();
            m_timerTicks = 0;
        }
    }

    for (auto itr = m_consumersInfo.begin(); itr != m_consumersInfo.end(); ++itr)
    {
        auto& info = *itr;

        // Do not handle recently added items, some start animation can be in progress.
        if (!info.initialTime.hasExpired(kRecentlyAddedIntervalMs))
            continue;

        QnCamDisplay* display = itr.key();

        // Omit cameras without dual streaming, offline and non-authorized cameras.
        if (!isSupportedDisplay(display))
            continue;

        // switch HQ->LQ if visual item size is small
        QnArchiveStreamReader* reader = display->getArchiveReader();

        if (isForcedHQDisplay(display, reader) && !isFFSpeed(display))
        {
            reader->setQuality(MEDIA_Quality_High, true);
        }
        else if (info.awaitingLQTime.isValid()
            && info.awaitingLQTime.hasExpired(kQualitySwitchIntervalMs))
        {
            gotoLowQuality(display, display->queueSize() < 3 ? Reason_Network : Reason_CPU);
        }
        else
        {
            if (reader->getQuality() == MEDIA_Quality_High && isSmallItem(display) && !reader->isMediaPaused())
            {
                if (++info.smallSizeCnt > 1)
                {
                    gotoLowQuality(display, Reason_Small);
                    addHQTry();
                }
            }
            else
            {
                info.smallSizeCnt = 0;
            }
        }
        // switch LQ->HQ for LIVE here
        if (display->isRealTimeSource()
            && display->getArchiveReader()->getQuality() == MEDIA_Quality_Low  // LQ live stream
            && qnSyncTime->currentMSecsSinceEpoch() - m_consumersInfo[display].lqTime > kQualitySwitchIntervalMs  // no drop report several last seconds
            && display->queueSize() <= display->maxQueueSize() / 2 // there are no a lot of packets in the queue (it is possible if CPU slow)
            && !m_lastSwitchTimer.hasExpired(kQualitySwitchIntervalMs) // no recently slow report by any camera
            && !isSmallItem2(display))  // is big enough item for HQ
        {
            streamBackToNormal(display->getArchiveReader());
        }
    }

    optimizeItemsQualityBySize();
}

void RadassController::optimizeItemsQualityBySize()
{
    // rearrange items quality: put small items to LQ state, large to HQ

    if (!m_lastSwitchTimer.hasExpired(kQualitySwitchIntervalMs))
        return; // do not optimize quality if recently switch occurred

    for (auto itr = m_consumersInfo.begin(); itr != m_consumersInfo.end(); ++itr)
    {
        QnCamDisplay* display = itr.key();
        if (display->getSpeed() > 1 || display->getSpeed() < 0)
            return; // do not rearrange items if FF/REW mode
    }

    int largeSize = 0;
    int smallSize = 0;
    QnCamDisplay* largeDisplay = findDisplay(Find_Biggest, MEDIA_Quality_Low, &RadassController::itemCanBeOptimized, &largeSize);
    QnCamDisplay* smallDisplay = findDisplay(Find_Least, MEDIA_Quality_High, &RadassController::itemCanBeOptimized, &smallSize);
    if (largeDisplay && smallDisplay && largeSize >= smallSize * 2)
    {
        // swap items quality
        gotoLowQuality(smallDisplay, m_consumersInfo[largeDisplay].lqReason);
        largeDisplay->getArchiveReader()->setQuality(MEDIA_Quality_High, true);
        m_lastSwitchTimer.restart();
    }
}

int RadassController::counsumerCount() const
{
    QnMutexLocker lock(&m_mutex);
    return m_consumersInfo.size();
}

void RadassController::registerConsumer(QnCamDisplay* display)
{
    QnMutexLocker lock(&m_mutex);
    QnArchiveStreamReader* reader = display->getArchiveReader();
    if (display->getArchiveReader())
    {
        if (isSupportedDisplay(display))
        {
            switch (m_mode)
            {
                case RadassMode::Auto:
                    if (!isForcedHQDisplay(display, reader))
                    {
                        if (m_consumersInfo.size() >= 16)
                        {
                            gotoLowQuality(display, Reason_Network);
                        }
                        else
                        {
                            for (auto itr = m_consumersInfo.begin(); itr != m_consumersInfo.end(); ++itr)
                            {
                                if (itr.key()->getArchiveReader()->getQuality() == MEDIA_Quality_Low && itr.value().lqReason != Reason_Small)
                                    gotoLowQuality(display, itr.value().lqReason);
                            }
                        }
                    }
                    break;

                case RadassMode::High:
                    display->getArchiveReader()->setQuality(MEDIA_Quality_High, true);
                    break;
                case RadassMode::Low:
                    display->getArchiveReader()->setQuality(MEDIA_Quality_Low, true);
                    break;
                default:
                    break;
            }
        }
        m_consumersInfo.insert(display, ConsumerInfo());
    }
}

void RadassController::gotoLowQuality(QnCamDisplay* display, LQReason reason, double speed)
{
    LQReason oldReason = m_consumersInfo[display].lqReason;
    if ((oldReason == Reason_Network || oldReason == Reason_CPU) && (reason == Reason_Network || reason == Reason_CPU))
        m_hiQualityRetryCounter++; // item goes to LQ again because not enough resources

    display->getArchiveReader()->setQuality(MEDIA_Quality_Low, true);
    m_consumersInfo[display].lqReason = reason;
    m_consumersInfo[display].toLQSpeed = speed != INT_MAX ? speed : display->getSpeed(); // get speed for FF reason as external varialbe to prevent race condition
    m_consumersInfo[display].awaitingLQTime.invalidate();
}

void RadassController::unregisterConsumer(QnCamDisplay* display)
{
    QnMutexLocker lock(&m_mutex);
    if (!m_consumersInfo.contains(display))
        return;
    m_consumersInfo.remove(display);
    addHQTry();
}

void RadassController::addHQTry()
{
    m_hiQualityRetryCounter = qMin(m_hiQualityRetryCounter, kHighQualityRetryCounter);
    m_hiQualityRetryCounter = qMax(0, m_hiQualityRetryCounter - 1);
}

void RadassController::setMode(RadassMode mode)
{
    QnMutexLocker lock(&m_mutex);

    if (m_mode == mode)
        return;

    m_mode = mode;

    if (m_mode == RadassMode::Auto)
    {
        m_hiQualityRetryCounter = 0; // allow LQ->HQ switching
    }
    else
    {
        for (auto itr = m_consumersInfo.begin(); itr != m_consumersInfo.end(); ++itr)
        {
            QnCamDisplay* display = itr.key();

            if (!isSupportedDisplay(display))
                continue; // ommit cameras without dual streaming, offline and non-authorized cameras

            QnArchiveStreamReader* reader = display->getArchiveReader();
            reader->setQuality(m_mode == RadassMode::High ? MEDIA_Quality_High : MEDIA_Quality_Low, true);
        }
    }
}

RadassMode RadassController::getMode() const
{
    QnMutexLocker lock(&m_mutex);
    return m_mode;
}

} // namespace desktop
} // namespace client
} // namespace nx
