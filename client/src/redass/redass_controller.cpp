#include "redass_controller.h"
#include "camera/cam_display.h"
#include "plugins/resource/archive/archive_stream_reader.h"
#include "core/resource/camera_resource.h"

Q_GLOBAL_STATIC(QnRedAssController, inst);

static const int QUALITY_SWITCH_INTERVAL = 1000 * 5; // delay between quality switching attempts
static const int HIGH_QUALITY_RETRY_COUNTER = 1;
static const QSize TO_LOWQ_SCREEN_SIZE(320/1.4,240/1.4);      // put item to LQ if visual size is small


static const int TIMER_TICK_INTERVAL = 500; // at ms
static const int TOHQ_ADDITIONAL_TRY = 10*60*1000 / TIMER_TICK_INTERVAL; // every 10 min
static const double FPS_EPS = 0.0001;
static const double LQ_HQ_THRESHOLD = 1.34;

QnRedAssController* QnRedAssController::instance()
{
    return inst();
}

QnRedAssController::QnRedAssController(): m_mutex(QMutex::Recursive), m_mode(Qn::AutoResolution)
{
    QObject::connect(&m_timer, SIGNAL(timeout()), this, SLOT(onTimer()));
    m_timer.start(TIMER_TICK_INTERVAL);
    m_lastSwitchTimer.start();
    m_hiQualityRetryCounter = 0;
    m_timerTicks = 0;
    m_lastLqTime = 0;
}

QnCamDisplay* QnRedAssController::getDisplayByReader(QnArchiveStreamReader* reader)
{
    for(QMap<QnCamDisplay*, RedAssInfo>::iterator itr = m_redAssInfo.begin(); itr != m_redAssInfo.end(); ++itr)
    {
        QnCamDisplay* display = itr.key();
        if (display->getArchiveReader() == reader)
            return display;
    }
    return 0;
}

bool QnRedAssController::isSupportedDisplay(QnCamDisplay* display) const
{
    if (!display || !display->getArchiveReader())
        return false;
    QnSecurityCamResourcePtr cam = display->getArchiveReader()->getResource().dynamicCast<QnSecurityCamResource>();
    return cam && cam->hasDualStreaming();
}

QnCamDisplay* QnRedAssController::findDisplay(FindMethod method, MediaQuality findQuality, SearchCondition cond, int* displaySize)
{
    bool findHQ = findQuality == MEDIA_Quality_High;
    if (displaySize)
        *displaySize = 0;
    QMultiMap<qint64, QnCamDisplay*> camDisplayByScreenSize;
    for (ConsumersMap::iterator itr = m_redAssInfo.begin(); itr != m_redAssInfo.end(); ++itr)
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
        if (isReaderHQ == findHQ && (!cond || (this->*cond)(display))) {
            if (displaySize)
                *displaySize = itr.key() >> 32;
            return display;
        }
    }
    return 0;
}

bool QnRedAssController::isForcedHQDisplay(QnCamDisplay* display, QnArchiveStreamReader* reader) const
{
    return display->isFullScreen() || display->isZoomWindow() || display->isFisheyeEnabled(); 
}

void QnRedAssController::onSlowStream(QnArchiveStreamReader* reader)
{
    QMutexLocker lock(&m_mutex);

    if (m_mode != Qn::AutoResolution)
        return;

    QnCamDisplay* display = getDisplayByReader(reader);
    if (!isSupportedDisplay(display))
        return;
    m_lastLqTime = m_redAssInfo[display].lqTime = qnSyncTime->currentMSecsSinceEpoch();
    
    
    double speed = display->getSpeed();
    if (isFFSpeed(speed))
    {
        // for high speed mode change same item to LQ (do not try to find least item)
        gotoLowQuality(display, Reson_FF, speed);
        return;
    }
    
    if (isForcedHQDisplay(display, reader))
        return;

    if (reader->getQuality() == MEDIA_Quality_Low)
        return; // reader already at LQ

    if (m_lastSwitchTimer.elapsed() < QUALITY_SWITCH_INTERVAL) {
        m_redAssInfo[display].awaitingLQTime = qnSyncTime->currentMSecsSinceEpoch();
        return; // do not go to LQ if recently switch occurred
    }

    // switch to LQ min item
    display = findDisplay(Find_Least, MEDIA_Quality_High);
    if (display) {
//        QnArchiveStreamReader* reader = display->getArchiveReader();
        gotoLowQuality(display, display->queueSize() < 3 ? Reason_Network : Reason_CPU);
        m_lastSwitchTimer.restart();
    }
}

bool QnRedAssController::existstBufferingDisplay() const
{
    for (ConsumersMap::const_iterator itr = m_redAssInfo.constBegin(); itr != m_redAssInfo.constEnd(); ++itr)
    {
        const QnCamDisplay* display = itr.key();
        if (display->isBuffering())
            return true;
    }
    return false;
}

void QnRedAssController::streamBackToNormal(QnArchiveStreamReader* reader)
{
    QMutexLocker lock(&m_mutex);

    if (m_mode != Qn::AutoResolution)
        return;

    if (reader->getQuality() == MEDIA_Quality_High || reader->getQuality() == MEDIA_Quality_ForceHigh)
        return; // reader already at HQ
    
    QnCamDisplay* display = getDisplayByReader(reader);
    if (!isSupportedDisplay(display))
        return;

    m_redAssInfo[display].awaitingLQTime = 0;

    if (qAbs(display->getSpeed()) < m_redAssInfo[display].toLQSpeed || (m_redAssInfo[display].toLQSpeed < 0 && display->getSpeed() > 0))
    {
        // If item leave high speed mode change same item to HQ (do not try to find biggest item)
        reader->setQuality(MEDIA_Quality_High, true);
        return;
    }
    else if (isFFSpeed(display))
        return; // do not return to HQ in FF mode because of retry counter is not increased for FF

    if (m_hiQualityRetryCounter >= HIGH_QUALITY_RETRY_COUNTER)
        return; // Some item stuck after HQ switching. Do not switch to HQ any more

    if (isSmallItem2(display))
        return; // do not try HQ for small items

    if (m_redAssInfo[display].lqReason == Reason_Small)
    {
        reader->setQuality(MEDIA_Quality_High, true);
        return; // if item go to LQ because of small, return to HQ without delay
    }

    //try one more LQ->HQ switch

    if (m_lastSwitchTimer.elapsed() < QUALITY_SWITCH_INTERVAL)
        return; // recently LQ->HQ or HQ->LQ
    if (m_lastLqTime + QUALITY_SWITCH_INTERVAL > qnSyncTime->currentMSecsSinceEpoch())
        return; // recently slow report received (not all reports affect m_lastSwitchTimer)

    if (existstBufferingDisplay())
        return; // do not go to HQ if some display perform opening...

    display = findDisplay(Find_Biggest, MEDIA_Quality_Low, &QnRedAssController::isNotSmallItem2);
    if (display) {
        display->getArchiveReader()->setQuality(MEDIA_Quality_High, true);
        m_lastSwitchTimer.restart();
    }
}

bool QnRedAssController::isSmallItem(QnCamDisplay* display)
{
    QSize sz = display->getMaxScreenSize();
    return sz.height() <= TO_LOWQ_SCREEN_SIZE.height();
}

bool QnRedAssController::isSmallItem2(QnCamDisplay* display)
{
    QSize sz = display->getMaxScreenSize();
    return sz.height() <= TO_LOWQ_SCREEN_SIZE.height() * LQ_HQ_THRESHOLD;
}

bool QnRedAssController::itemCanBeOptimized(QnCamDisplay* display)
{
    QnArchiveStreamReader* reader = display->getArchiveReader();
    return !isSmallItem(display) && !isForcedHQDisplay(display, reader);
}

bool QnRedAssController::isNotSmallItem2(QnCamDisplay* display)
{
    return !isSmallItem2(display);
}

bool QnRedAssController::isFFSpeed(QnCamDisplay* display) const
{
    return isFFSpeed(display->getSpeed());
}

bool QnRedAssController::isFFSpeed(double speed) const
{
    return speed > 1 + FPS_EPS || speed < 0;
}

void QnRedAssController::onTimer()
{
    QMutexLocker lock(&m_mutex);

    if (m_mode != Qn::AutoResolution) 
    {
        for (ConsumersMap::iterator itr = m_redAssInfo.begin(); itr != m_redAssInfo.end(); ++itr)
        {
            QnCamDisplay* display = itr.key();
            QnArchiveStreamReader* reader = display->getArchiveReader();
            if (!display->isFullScreen())
                reader->setQuality(m_mode == Qn::LowResolution ? MEDIA_Quality_Low : MEDIA_Quality_High, true);
        }
        return;
    }

    if (++m_timerTicks >= TOHQ_ADDITIONAL_TRY)
    {
        // sometimes allow addition LQ->HQ switch
        bool slowStreamExists = false;
        for (ConsumersMap::iterator itr = m_redAssInfo.begin(); itr != m_redAssInfo.end(); ++itr)
        {
            if (qnSyncTime->currentMSecsSinceEpoch() < itr.value().lqTime  + QUALITY_SWITCH_INTERVAL)
                slowStreamExists = true;
        }
        if (!slowStreamExists) {
            addHQTry(); 
            m_timerTicks = 0;
        }
    }

    //if (m_lastSwitchTimer.elapsed() < QUALITY_SWITCH_INTERVAL)
    //    return;

    for (ConsumersMap::iterator itr = m_redAssInfo.begin(); itr != m_redAssInfo.end(); ++itr)
    {
        if (qnSyncTime->currentMSecsSinceEpoch() - itr.value().initialTime < 1000)
            continue; // do not handle recently added items, some start animation can be in progress

        QnCamDisplay* display = itr.key();

        if (!isSupportedDisplay(display))
            continue; // omit cameras without dual streaming, offline and non-authorized cameras

        // switch HQ->LQ if visual item size is small
        QnArchiveStreamReader* reader = display->getArchiveReader();

        if (isForcedHQDisplay(display, reader) && !isFFSpeed(display))
            reader->setQuality(MEDIA_Quality_High, true);
        else if (itr.value().awaitingLQTime && qnSyncTime->currentMSecsSinceEpoch() - itr.value().awaitingLQTime > QUALITY_SWITCH_INTERVAL)
            gotoLowQuality(display, display->queueSize() < 3 ? Reason_Network : Reason_CPU);
        else {
            if (reader->getQuality() == MEDIA_Quality_High && isSmallItem(display) && !reader->isMediaPaused())
            {
                if (++itr.value().smallSizeCnt > 1) {
                    gotoLowQuality(display, Reason_Small);
                    addHQTry();
                }
            }
            else {
                itr.value().smallSizeCnt = 0;
            }
        }
        // switch LQ->HQ for LIVE here
        if (display->isRealTimeSource() && display->getArchiveReader()->getQuality() == MEDIA_Quality_Low &&   // LQ live stream
            qnSyncTime->currentMSecsSinceEpoch() - m_redAssInfo[display].lqTime > QUALITY_SWITCH_INTERVAL &&  // no drop report several last seconds
            display->queueSize() <= display->maxQueueSize()/2 && // there are no a lot of packets in the queue (it is possible if CPU slow)
            m_lastSwitchTimer.elapsed() >= QUALITY_SWITCH_INTERVAL && // no recently slow report by any camera
            !isSmallItem2(display))  // is big enough item for HQ
        {
            streamBackToNormal(display->getArchiveReader());
        }
    }

    optimizeItemsQualityBySize();
}

void QnRedAssController::optimizeItemsQualityBySize()
{
    // rearrange items quality: put small items to LQ state, large to HQ
 
    if (m_lastSwitchTimer.elapsed() < QUALITY_SWITCH_INTERVAL)
        return; // do not optimize quality if recently switch occurred

    for (ConsumersMap::iterator itr = m_redAssInfo.begin(); itr != m_redAssInfo.end(); ++itr)
    {
        QnCamDisplay* display = itr.key();
        if (display->getSpeed() > 1 || display->getSpeed() < 0)
            return; // do not rearrange items if FF/REW mode
    }

    int largeSize = 0;
    int smallSize = 0;
    QnCamDisplay* largeDisplay = findDisplay(Find_Biggest, MEDIA_Quality_Low, &QnRedAssController::itemCanBeOptimized, &largeSize);
    QnCamDisplay* smallDisplay = findDisplay(Find_Least, MEDIA_Quality_High, &QnRedAssController::itemCanBeOptimized, &smallSize);
    if (largeDisplay && smallDisplay && largeSize >= smallSize*2)
    {
        // swap items quality
        gotoLowQuality(smallDisplay, m_redAssInfo[largeDisplay].lqReason);
        largeDisplay->getArchiveReader()->setQuality(MEDIA_Quality_High, true);
        m_lastSwitchTimer.restart();
    }
}

int QnRedAssController::counsumerCount() const
{
    QMutexLocker lock(&m_mutex);
    return m_redAssInfo.size();
}

void QnRedAssController::registerConsumer(QnCamDisplay* display)
{
    QMutexLocker lock(&m_mutex);
    QnArchiveStreamReader* reader = display->getArchiveReader();
    if (display->getArchiveReader()) 
    {
        if (isSupportedDisplay(display))
        {
            switch (m_mode) 
            {
            case Qn::AutoResolution:
                if (!isForcedHQDisplay(display, reader))
                {
                    if (m_redAssInfo.size() >= 16) {
                        gotoLowQuality(display, Reason_Network);
                    }
                    else {
                        for (ConsumersMap::iterator itr = m_redAssInfo.begin(); itr != m_redAssInfo.end(); ++itr)
                        {
                            if (itr.key()->getArchiveReader()->getQuality() == MEDIA_Quality_Low && itr.value().lqReason != Reason_Small)
                                gotoLowQuality(display, itr.value().lqReason);
                        }
                    }
                }
                break;

            case Qn::HighResolution:
                display->getArchiveReader()->setQuality(MEDIA_Quality_High, true);
                break;
            case Qn::LowResolution:
                display->getArchiveReader()->setQuality(MEDIA_Quality_Low, true);
                break;
            default:
                break;
            }
        }
        m_redAssInfo.insert(display, RedAssInfo());
    }
}

void QnRedAssController::gotoLowQuality(QnCamDisplay* display, LQReason reason, double speed)
{
    LQReason oldReason = m_redAssInfo[display].lqReason;
    if ((oldReason == Reason_Network || oldReason == Reason_CPU) && (reason == Reason_Network || reason == Reason_CPU))
        m_hiQualityRetryCounter++; // item goes to LQ again because not enough resources

    display->getArchiveReader()->setQuality(MEDIA_Quality_Low, true);
    m_redAssInfo[display].lqReason = reason;
    m_redAssInfo[display].toLQSpeed = speed != INT_MAX ? speed : display->getSpeed(); // get speed for FF reason as external varialbe to prevent race condition
    m_redAssInfo[display].awaitingLQTime = 0;
}

void QnRedAssController::unregisterConsumer(QnCamDisplay* display)
{
    QMutexLocker lock(&m_mutex);
    if (!m_redAssInfo.contains(display))
        return;
    m_redAssInfo.remove(display);
    addHQTry();
}

void QnRedAssController::addHQTry()
{
    m_hiQualityRetryCounter = qMin(m_hiQualityRetryCounter, HIGH_QUALITY_RETRY_COUNTER);
    m_hiQualityRetryCounter = qMax(0, m_hiQualityRetryCounter-1);
}

void QnRedAssController::setMode(Qn::ResolutionMode mode)
{
    QMutexLocker lock(&m_mutex);

    if (m_mode == mode)
        return;

    m_mode = mode;

    if (m_mode == Qn::AutoResolution) {
        m_hiQualityRetryCounter = 0; // allow LQ->HQ switching
    }
    else {
        for (ConsumersMap::iterator itr = m_redAssInfo.begin(); itr != m_redAssInfo.end(); ++itr)
        {
            QnCamDisplay* display = itr.key();

            if (!isSupportedDisplay(display))
                continue; // ommit cameras without dual streaming, offline and non-authorized cameras

            QnArchiveStreamReader* reader = display->getArchiveReader();
            reader->setQuality(m_mode == Qn::HighResolution ? MEDIA_Quality_High : MEDIA_Quality_Low, true);
        }
    }
}

Qn::ResolutionMode QnRedAssController::getMode() const
{
    return m_mode;
}
