#include "redass_controller.h"
#include "camera/cam_display.h"
#include "plugins/resources/archive/archive_stream_reader.h"
#include "core/resource/camera_resource.h"

Q_GLOBAL_STATIC(QnRedAssController, inst);

static const int QUALITY_SWITCH_INTERVAL = 1000 * 5; // delay between quality switching attempts
static const int HIGH_QUALITY_RETRY_COUNTER = 1;
static const QSize TO_LOWQ_SCREEN_SIZE(320/1.4,240/1.4);      // put item to LQ if visual size is small


static const int TIMER_TICK_INTERVAL = 500; // at ms
static const int TOHQ_ADDITIONAL_TRY = 10*60*1000 / TIMER_TICK_INTERVAL; // every 10 min

QnRedAssController* QnRedAssController::instance()
{
    return inst();
}

QnRedAssController::QnRedAssController(): m_mutex(QMutex::Recursive)
{
    connect(&m_timer, SIGNAL(timeout()), this, SLOT(onTimer()));
    m_timer.start(TIMER_TICK_INTERVAL);
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
    QnSecurityCamResourcePtr cam = display->getArchiveReader()->getResource().dynamicCast<QnSecurityCamResource>();
    return cam && cam->hasDualStreaming() && cam->getStatus() != QnResource::Offline && cam->getStatus() != QnResource::Unauthorized;
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

        QSize size = display->getScreenSize();
        QSize res = display->getVideoSize();
        qint64 screenSquare = size.width() * size.height();
        int pps = res.width()*res.height()*display->getAvarageFps(); // pps - pixels per second
        pps = INT_MAX - pps; // put display with max pps to the begin, so if slow stream do HQ->LQ for max pps disply (between displays with same size)
        camDisplayByScreenSize.insert((screenSquare << 32) + pps, display);
    }

    QMapIterator<qint64, QnCamDisplay*> itr(camDisplayByScreenSize);
    if (method == Find_Biggest)
        itr.toBack();

    while (method == Find_Least && itr.hasNext() || method == Find_Biggest && itr.hasPrevious())
    {
        if (method == Find_Least)
            itr.next();
        else
            itr.previous();
        QnCamDisplay* display = itr.value();
        QnArchiveStreamReader* reader = display->getArchiveReader();
        bool isReaderHQ = reader->getQuality() == MEDIA_Quality_High;
        if (isReaderHQ == findHQ && (!cond || (this->*cond)(display))) {
            if (displaySize)
                *displaySize = itr.key() >> 32;
            return display;
        }
    }
    return 0;
}

void QnRedAssController::onSlowStream(QnArchiveStreamReader* reader)
{
    QMutexLocker lock(&m_mutex);

    QnCamDisplay* display = getDisplayByReader(reader);
    if (!isSupportedDisplay(display))
        return;
    m_lastLqTime = m_redAssInfo[display].lqTime = qnSyncTime->currentMSecsSinceEpoch();
    
    if (display->getSpeed() > 1.0 || display->getSpeed() < 0)
    {
        // for high speed mode change same item to LQ (do not try to find least item)
        gotoLowQuality(display, Reson_FF);
        return;
    }
    
    if (display->isFullScreen())
        return; // do not go to LQ for full screen items (except of FF/REW play)

    if (m_lastSwitchTimer.elapsed() < QUALITY_SWITCH_INTERVAL)
        return; // do not go to LQ if recently switch occurred
    if (reader->getQuality() == MEDIA_Quality_Low)
        return; // reader already at LQ

    // switch to LQ min item
    display = findDisplay(Find_Least, MEDIA_Quality_High);
    if (display) {
        QnArchiveStreamReader* reader = display->getArchiveReader();
        gotoLowQuality(display, display->queueSize() < 3 ? Reason_Network : Reason_CPU);
        m_lastSwitchTimer.restart();
    }
}

void QnRedAssController::streamBackToNormal(QnArchiveStreamReader* reader)
{
    QMutexLocker lock(&m_mutex);

    if (reader->getQuality() == MEDIA_Quality_High)
        return; // reader already at HQ
    
    QnCamDisplay* display = getDisplayByReader(reader);
    if (!isSupportedDisplay(display))
        return;

    if (qAbs(display->getSpeed()) < m_redAssInfo[display].toLQSpeed || (m_redAssInfo[display].toLQSpeed < 0 && display->getSpeed() > 0))
    {
        // If item leave high speed mode change same item to HQ (do not try to find biggest item)
        reader->setQuality(MEDIA_Quality_High, true);
        return;
    }

    if (m_hiQualityRetryCounter >= HIGH_QUALITY_RETRY_COUNTER)
        return; // Some item stuck after HQ switching. Do not switch to HQ any more

    if (isSmallItem(display))
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

    display = findDisplay(Find_Biggest, MEDIA_Quality_Low, &QnRedAssController::isNotSmallItem);
    if (display) {
        display->getArchiveReader()->setQuality(MEDIA_Quality_High, true);
        m_lastSwitchTimer.restart();
    }
}

bool QnRedAssController::isSmallItem(QnCamDisplay* display)
{
    QSize sz = display->getScreenSize();
    return sz.height() <= TO_LOWQ_SCREEN_SIZE.height();
}

bool QnRedAssController::isNotSmallItem(QnCamDisplay* display)
{
    return !isSmallItem(display);
}

void QnRedAssController::onTimer()
{
    QMutexLocker lock(&m_mutex);

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
            continue; // do not hanlde recently added items, some start animation can be in progress

        QnCamDisplay* display = itr.key();

        if (!isSupportedDisplay(display))
            continue; // ommit cameras without dual streaming, offline and non-authorized cameras

        // switch HQ->LQ if visual item size is small
        QnArchiveStreamReader* reader = display->getArchiveReader();
        if (reader->getQuality() == MEDIA_Quality_High && isSmallItem(display))
        {
            gotoLowQuality(display, Reason_Small);
            addHQTry();
        }

        // switch LQ->HQ for LIVE here
        if (display->isRealTimeSource() && display->getArchiveReader()->getQuality() == MEDIA_Quality_Low &&   // LQ live stream
            qnSyncTime->currentMSecsSinceEpoch() - m_redAssInfo[display].lqTime > QUALITY_SWITCH_INTERVAL &&  // no drop report several last seconds
            display->queueSize() < 2 && // there are no a lot of packets in the queue (it is possible if CPU slow)
            m_lastSwitchTimer.elapsed() >= QUALITY_SWITCH_INTERVAL && // no recently slow report by any camera
            !isSmallItem(display))  // is big enough item for HQ
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
        return; // do not optimize quality if recently switch occured

    for (ConsumersMap::iterator itr = m_redAssInfo.begin(); itr != m_redAssInfo.end(); ++itr)
    {
        QnCamDisplay* display = itr.key();
        if (display->getSpeed() > 1 || display->getSpeed() < 0)
            return; // do not rearrange items if FF/REW mode
    }

    int largeSize = 0;
    int smallSize = 0;
    QnCamDisplay* largeDisplay = findDisplay(Find_Biggest, MEDIA_Quality_Low, &QnRedAssController::isNotSmallItem, &largeSize);
    QnCamDisplay* smallDisplay = findDisplay(Find_Least, MEDIA_Quality_High, &QnRedAssController::isNotSmallItem, &smallSize);
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
    if (display->getArchiveReader()) 
    {
        for (ConsumersMap::iterator itr = m_redAssInfo.begin(); itr != m_redAssInfo.end(); ++itr)
        {
            if (itr.key()->getArchiveReader()->getQuality() == MEDIA_Quality_Low && itr.value().lqReason != Reason_Small)
                gotoLowQuality(display, itr.value().lqReason);
        }
        m_redAssInfo.insert(display, RedAssInfo());
    }
}

void QnRedAssController::gotoLowQuality(QnCamDisplay* display, LQReason reason)
{
    LQReason oldReason = m_redAssInfo[display].lqReason;
    if ((oldReason == Reason_Network || oldReason == Reason_CPU) && (reason == Reason_Network || reason == Reason_CPU))
        m_hiQualityRetryCounter++; // item goes to LQ again because not enough resources

    display->getArchiveReader()->setQuality(MEDIA_Quality_Low, true);
    m_redAssInfo[display].lqReason = reason;
    m_redAssInfo[display].toLQSpeed = display->getSpeed();
}

void QnRedAssController::unregisterConsumer(QnCamDisplay* display)
{
    QMutexLocker lock(&m_mutex);
    m_redAssInfo.remove(display);
    addHQTry();
}

void QnRedAssController::addHQTry()
{
    m_hiQualityRetryCounter = qMin(m_hiQualityRetryCounter, HIGH_QUALITY_RETRY_COUNTER);
    m_hiQualityRetryCounter = qMax(0, m_hiQualityRetryCounter-1);
}
