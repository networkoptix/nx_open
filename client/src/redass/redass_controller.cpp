#include "redass_controller.h"
#include "camera/cam_display.h"
#include "utils/common/synctime.h"
#include "plugins/resources/archive/archive_stream_reader.h"

Q_GLOBAL_STATIC(QnRedAssController, inst);

static const int QUALITY_SWITCH_INTERVAL = 1000 * 5; // delay between high quality switching attempts

QnRedAssController* QnRedAssController::instance()
{
    return inst();
}

QnRedAssController::QnRedAssController()
{
    connect(&m_timer, SIGNAL(timeout()), this, SLOT(onTimer()));
    m_timer.start(500);
}

void QnRedAssController::onTimer()
{

}

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

void QnRedAssController::onSlowStream(QnArchiveStreamReader* reader)
{
    QMutexLocker lock(&m_mutex);

    if (m_lastSwitchTimer.elapsed() < QUALITY_SWITCH_INTERVAL)
        return;

    QMap<int, QnCamDisplay*> camDisplayByScreenSize;
    foreach(QnCamDisplay* display, m_consumers) {
        QSize size = display->getScreenSize();
        camDisplayByScreenSize.insert(size.width() * size.height(), display);
    }

    // switch to LQ min item
    for(QMap<int, QnCamDisplay*>::Iterator itr = camDisplayByScreenSize.begin(); itr != camDisplayByScreenSize.end(); ++itr)
    {
        QnCamDisplay* display = itr.value();
        QnArchiveStreamReader* reader = display->getArchiveReader();
        if (reader && reader->getQuality() == MEDIA_Quality_High)
        {
            reader->setQuality(MEDIA_Quality_Low, true);
        }
    }
    m_lastSwitchTimer.restart();
}

void QnRedAssController::registerConsumer(QnCamDisplay* display)
{
    QMutexLocker lock(&m_mutex);
    m_consumers << display;
}

void QnRedAssController::unregisterConsumer(QnCamDisplay* display)
{
    QMutexLocker lock(&m_mutex);
    m_consumers.remove(display);
}
