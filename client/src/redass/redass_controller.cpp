#include "redass_controller.h"
#include "camera/cam_display.h"
#include "plugins/resources/archive/archive_stream_reader.h"

Q_GLOBAL_STATIC(QnRedAssController, inst);

static const int QUALITY_SWITCH_INTERVAL = 1000 * 5; // delay between high quality switching attempts
static const int HIGH_QUALITY_RETRY_COUNTER = 1;
static const int TO_LOWQ_SCREEN_SIZE = 320*240;

static const int TIMER_TICK_INTERVAL = 500; // at ms
static const int TOHQ_ADDITIONAL_TRY = 10*60*1000 / TIMER_TICK_INTERVAL; // every 10 min

QnRedAssController* QnRedAssController::instance()
{
    return inst();
}

QnRedAssController::QnRedAssController()
{
    connect(&m_timer, SIGNAL(timeout()), this, SLOT(onTimer()));
    m_timer.start(TIMER_TICK_INTERVAL);
    m_hiQualityRetryCounter = 0;
    m_timerTicks = 0;
}

/*
void QnRedAssController::setQuality(QnCamDisplay* display, MediaQuality quality)
{
    if (quality == MEDIA_Quality_Low)
    {
        m_lowQualityRequests.insert(display, qnSyncTime->currentMSecsSinceEpoch());
    }
    else if (quality == MEDIA_Quality_High || quality == MEDIA_Quality_AlwaysHigh) 
    {
        m_lowQualityRequests.remove(display);
        if (display->getArchiveReader())
            display->getArchiveReader()->setQualityForced(quality);
    }   
}
*/

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

QnCamDisplay* QnRedAssController::findDisplay(FindMethod method, bool findHQ, SearchCondition cond)
{
    QMultiMap<int, QnCamDisplay*> camDisplayByScreenSize;
    for (ConsumersMap::iterator itr = m_redAssInfo.begin(); itr != m_redAssInfo.end(); ++itr)
    {
        QnCamDisplay* display = itr.key();
        QSize size = display->getScreenSize();
        camDisplayByScreenSize.insert(size.width() * size.height(), display);
    }

    QMapIterator<int, QnCamDisplay*> itr(camDisplayByScreenSize);
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
        if (isReaderHQ == findHQ && (!cond || (this->*cond)(display)))
            return display;
    }
    return 0;
}

void QnRedAssController::onSlowStream(QnArchiveStreamReader* reader)
{
    QMutexLocker lock(&m_mutex);

    QnCamDisplay* display = getDisplayByReader(reader);
    m_redAssInfo[display].lqTime = qnSyncTime->currentMSecsSinceEpoch();
    if (display->getSpeed() > 1.0 || display->getSpeed() < 0)
    {
        // for high speed mode change same item to LQ (do not try to find least item)
        gotoLowQuality(display, Reson_FF);
        return;
    }
    
    if (display->isFullScreen())
        return; // do not go to LQ for full screen items (except of FF/REW play)

    if (m_lastSwitchTimer.elapsed() < QUALITY_SWITCH_INTERVAL)
        return;
    if (reader->getQuality() == MEDIA_Quality_Low)
        return; // reader already at LQ

    // switch to LQ min item
    display = findDisplay(Find_Least, true);
    if (display) {
        QnArchiveStreamReader* reader = display->getArchiveReader();
        gotoLowQuality(display, display->queueSize() < 3 ? Reason_Network : Reason_CPU);
        m_lastSwitchTimer.restart();
    }
}

void QnRedAssController::streamBackToNormal(QnArchiveStreamReader* reader)
{
    if (reader->getQuality() == MEDIA_Quality_High)
        return; // reader already at HQ
    
    QnCamDisplay* display = getDisplayByReader(reader);
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
        return;

    display = findDisplay(Find_Biggest, false, &QnRedAssController::isNotSmallItem);
    if (display) {
        display->getArchiveReader()->setQuality(MEDIA_Quality_High, true);
        m_lastSwitchTimer.restart();
        //if (m_redAssInfo[display].lqTime > 0)
            //m_hiQualityRetryCounter++; // if was switch HQ > LQ (because of slow CPU or network), increase LQ->HQ try counter to prevent infinite HQ->LQ->HQ loop
    }
}

bool QnRedAssController::isSmallItem(QnCamDisplay* display)
{
    QSize sz = display->getScreenSize();
    return sz.width()*sz.height() <= TO_LOWQ_SCREEN_SIZE;
}

bool QnRedAssController::isNotSmallItem(QnCamDisplay* display)
{
    return !isSmallItem(display);
}

void QnRedAssController::onTimer()
{
    if (++m_timerTicks % TOHQ_ADDITIONAL_TRY == 0)
        addHQTry(); // sometimes allow addition LQ->HQ switch

    if (m_lastSwitchTimer.elapsed() < QUALITY_SWITCH_INTERVAL)
        return;

    for (ConsumersMap::iterator itr = m_redAssInfo.begin(); itr != m_redAssInfo.end(); ++itr)
    {
        if (qnSyncTime->currentMSecsSinceEpoch() - itr.value().initialTime < 1000)
            continue; // do not hanlde recently added items, some start animation can be in progress

        QnCamDisplay* display = itr.key();

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
            !isSmallItem(display))  // is big enough item for HQ
        {
            streamBackToNormal(display->getArchiveReader());
        }

    }
}

void QnRedAssController::registerConsumer(QnCamDisplay* display)
{
    QMutexLocker lock(&m_mutex);
    if (display->getArchiveReader()) {
        updateStreamQuality(display);
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
    m_redAssInfo[display].lqTime = qnSyncTime->currentMSecsSinceEpoch();
}

void QnRedAssController::updateStreamQuality(QnCamDisplay* display)
{
    for (ConsumersMap::iterator itr = m_redAssInfo.begin(); itr != m_redAssInfo.end(); ++itr)
    {
        if (itr.key()->getArchiveReader()->getQuality() == MEDIA_Quality_Low && itr.value().lqReason != Reason_Small)
            gotoLowQuality(display, itr.value().lqReason);
    }
}

void QnRedAssController::unregisterConsumer(QnCamDisplay* display)
{
    QMutexLocker lock(&m_mutex);
    m_redAssInfo.remove(display);
    addHQTry();
}

void QnRedAssController::addHQTry()
{
    m_hiQualityRetryCounter = qMax(0, m_hiQualityRetryCounter-1);
}
