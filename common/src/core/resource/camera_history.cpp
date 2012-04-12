#include "camera_history.h"
#include "core/resourcemanagment/resource_pool.h"
#include "video_server.h"

QString QnCameraHistory::getMacAddress() const
{
    return m_macAddress;
}

void QnCameraHistory::setMacAddress(const QString& macAddress)
{
    m_macAddress = macAddress;
}

QnCameraTimePeriodList QnCameraHistory::getTimePeriods() const
{
    return m_timePeriods;
}

QnCameraTimePeriodList::const_iterator QnCameraHistory::getVideoServerOnTimeItr(qint64 timestamp, bool searchForward)
{
    if (m_timePeriods.isEmpty())
        return m_timePeriods.end();
    QnCameraTimePeriodList::const_iterator itr = qUpperBound(m_timePeriods.begin(), m_timePeriods.end(), timestamp);
    if (itr != m_timePeriods.begin())
        --itr;
    if (searchForward && !itr->containTime(timestamp))
        ++itr;
    return itr;
}

QnVideoServerResourcePtr QnCameraHistory::getVideoServerOnTime(qint64 timestamp, bool searchForward, QnTimePeriod& currentPeriod)
{
    QnCameraTimePeriodList::const_iterator itr = getVideoServerOnTimeItr(timestamp, searchForward);
    if (itr == m_timePeriods.end())
        return QnVideoServerResourcePtr();
    currentPeriod = *itr;
    return qSharedPointerDynamicCast<QnVideoServerResource> (qnResPool->getResourceById(itr->videoServerId));
}

QnVideoServerResourcePtr QnCameraHistory::getNextVideoServerFromTime(qint64 timestamp, QnTimePeriod& currentPeriod)
{
    QnCameraTimePeriodList::const_iterator itr = getVideoServerOnTimeItr(timestamp, true);
    if (itr == m_timePeriods.end())
        return QnVideoServerResourcePtr();
    ++itr;
    if (itr == m_timePeriods.end())
        return QnVideoServerResourcePtr();
    currentPeriod = *itr;
    return qSharedPointerDynamicCast<QnVideoServerResource> (qnResPool->getResourceById(itr->videoServerId));
}

QnVideoServerResourcePtr QnCameraHistory::getPrevVideoServerFromTime(qint64 timestamp, QnTimePeriod& currentPeriod)
{
    QnCameraTimePeriodList::const_iterator itr = getVideoServerOnTimeItr(timestamp, false);
    if (itr == m_timePeriods.end() || itr == m_timePeriods.begin())
        return QnVideoServerResourcePtr();
    --itr;
    currentPeriod = *itr;
    return qSharedPointerDynamicCast<QnVideoServerResource> (qnResPool->getResourceById(itr->videoServerId));
}

QnVideoServerResourcePtr QnCameraHistory::getNextVideoServerOnTime(qint64 timestamp, bool searchForward, QnTimePeriod& currentPeriod)
{
    return searchForward ? getNextVideoServerFromTime(timestamp, currentPeriod) : getPrevVideoServerFromTime(timestamp, currentPeriod);
}


// ------------------- CameraHistory Pool ------------------------

Q_GLOBAL_STATIC(QnCameraHistoryPool, inst);

QnCameraHistoryPool* QnCameraHistoryPool::instance()
{
    return inst();
}

QnCameraHistoryPtr QnCameraHistoryPool::getCameraHistory(const QString& mac)
{
    return m_cameraHistory.value(mac);
}

void QnCameraHistoryPool::addCameraHistory(const QString& mac, QnCameraHistoryPtr history)
{
    m_cameraHistory.insert(mac, history);
}
