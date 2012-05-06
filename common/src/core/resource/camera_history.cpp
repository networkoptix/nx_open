#include "camera_history.h"
#include "core/resourcemanagment/resource_pool.h"
#include "video_server.h"

QnId QnCameraTimePeriod::getServerId() const
{
    QnId id;

    QnResourcePtr resource = qnResPool->getResourceByGuid(videoServerGuid);
    if (resource)
        id = resource->getId();

    return id;
}

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
    if (searchForward && timestamp > itr->endTimeMs())
        ++itr;
    return itr;
}

QnVideoServerResourcePtr QnCameraHistory::getVideoServerOnTime(qint64 timestamp, bool searchForward, QnTimePeriod& currentPeriod)
{
    QMutexLocker lock (&m_mutex);
    QnCameraTimePeriodList::const_iterator itr;
    if (timestamp == DATETIME_NOW && !m_timePeriods.isEmpty())
        itr = m_timePeriods.end()-1;
    else
        itr = getVideoServerOnTimeItr(timestamp, searchForward);
    if (itr == m_timePeriods.end())
        return QnVideoServerResourcePtr();
    currentPeriod = *itr;
    return qSharedPointerDynamicCast<QnVideoServerResource> (qnResPool->getResourceById(itr->getServerId()));
}

QnVideoServerResourcePtr QnCameraHistory::getNextVideoServerFromTime(qint64 timestamp, QnTimePeriod& currentPeriod)
{
    QMutexLocker lock (&m_mutex);
    QnCameraTimePeriodList::const_iterator itr = getVideoServerOnTimeItr(timestamp, true);
    if (itr == m_timePeriods.end())
        return QnVideoServerResourcePtr();
    ++itr;
    if (itr == m_timePeriods.end())
        return QnVideoServerResourcePtr();
    currentPeriod = *itr;
    return qSharedPointerDynamicCast<QnVideoServerResource> (qnResPool->getResourceById(itr->getServerId()));
}

QnVideoServerResourcePtr QnCameraHistory::getPrevVideoServerFromTime(qint64 timestamp, QnTimePeriod& currentPeriod)
{
    QMutexLocker lock (&m_mutex);
    QnCameraTimePeriodList::const_iterator itr = getVideoServerOnTimeItr(timestamp, false);
    if (itr == m_timePeriods.end() || itr == m_timePeriods.begin())
        return QnVideoServerResourcePtr();
    --itr;
    currentPeriod = *itr;
	qDebug() << "switch to previous video server. prevTime=" << QDateTime::fromMSecsSinceEpoch(timestamp).toString() << "newPeriod=" <<
		QDateTime::fromMSecsSinceEpoch(currentPeriod.startTimeMs).toString() << "-" << QDateTime::fromMSecsSinceEpoch(currentPeriod.endTimeMs()).toString();
    return qSharedPointerDynamicCast<QnVideoServerResource> (qnResPool->getResourceById(itr->getServerId()));
}

QnVideoServerResourcePtr QnCameraHistory::getNextVideoServerOnTime(qint64 timestamp, bool searchForward, QnTimePeriod& currentPeriod)
{
    return searchForward ? getNextVideoServerFromTime(timestamp, currentPeriod) : getPrevVideoServerFromTime(timestamp, currentPeriod);
}

void QnCameraHistory::addTimePeriod(const QnCameraTimePeriod& period)
{
    QMutexLocker lock (&m_mutex);

    // Works only if "period" startTimeMs is > the last item startTimeMs
    if (!m_timePeriods.isEmpty())
    {
        QnTimePeriod& lastItem = m_timePeriods.last();

        if (lastItem.durationMs == -1)
            lastItem.durationMs = period.startTimeMs - lastItem.startTimeMs;

    }

    m_timePeriods << period;
    //qSort(m_timePeriods.begin(), m_timePeriods.end());
}

qint64 QnCameraHistory::getMinTime() const
{
    QMutexLocker lock (&m_mutex);
    if (m_timePeriods.isEmpty())
        return AV_NOPTS_VALUE;
    return m_timePeriods.first().startTimeMs;
}

QList<QnNetworkResourcePtr> QnCameraHistory::getAllCamerasWithSameMac(const QnTimePeriod& timePeriod)
{
    QSet<QnNetworkResourcePtr> rez;

    QMutexLocker lock (&m_mutex);

    QnCameraTimePeriodList::const_iterator itrStart = getVideoServerOnTimeItr(timePeriod.startTimeMs, true);
    QnCameraTimePeriodList::const_iterator itrEnd = getVideoServerOnTimeItr(timePeriod.endTimeMs(), false);
    if (itrEnd != m_timePeriods.end())
        itrEnd++;

    for (QnCameraTimePeriodList::const_iterator itr = itrStart; itr < itrEnd; ++itr)
    {
        QString vServerId = itr->getServerId().toString();
        QnNetworkResourcePtr camera = qSharedPointerDynamicCast<QnNetworkResource> (qnResPool->getResourceByUniqId(m_macAddress + vServerId));
        if (camera)
            rez.insert(camera);
    }

    return rez.toList();
}
// ------------------- CameraHistory Pool ------------------------

Q_GLOBAL_STATIC(QnCameraHistoryPool, inst);

QnCameraHistoryPool* QnCameraHistoryPool::instance()
{
    return inst();
}

QnCameraHistoryPtr QnCameraHistoryPool::getCameraHistory(const QString& mac)
{
    QMutexLocker lock (&m_mutex);
    return m_cameraHistory.value(mac);
}

void QnCameraHistoryPool::addCameraHistory(QnCameraHistoryPtr history)
{
    QMutexLocker lock (&m_mutex);
    m_cameraHistory.insert(history->getMacAddress(), history);
}

QList<QnNetworkResourcePtr> QnCameraHistoryPool::getAllCamerasWithSameMac(QnNetworkResourcePtr camera, const QnTimePeriod& timePeriod)
{
    QnCameraHistoryPtr history = getCameraHistory(camera->getMAC().toString());
    if (!history)
        return QList<QnNetworkResourcePtr>() << camera;
    return history->getAllCamerasWithSameMac(timePeriod);
}

qint64 QnCameraHistoryPool::getMinTime(QnNetworkResourcePtr camera)
{
    if (!camera)
        return AV_NOPTS_VALUE;
    QnCameraHistoryPtr history = getCameraHistory(camera->getMAC().toString());
    if (!history)
        return AV_NOPTS_VALUE;

    return history->getMinTime();
}

void QnCameraHistoryPool::addCameraHistoryItem(const QnCameraHistoryItem &historyItem)
{
    QMutexLocker lock (&m_mutex);

    CameraHistoryMap::const_iterator iter = m_cameraHistory.find(historyItem.mac);

    QnCameraHistoryPtr cameraHistory;
    if (iter != m_cameraHistory.end())
        cameraHistory = iter.value();
    else {
        cameraHistory = QnCameraHistoryPtr(new QnCameraHistory());
        cameraHistory->setMacAddress(historyItem.mac);
    }

    QnCameraTimePeriod timePeriod(historyItem.timestamp, -1, historyItem.videoServerGuid);
    cameraHistory->addTimePeriod(timePeriod);

    m_cameraHistory[historyItem.mac] = cameraHistory;
}
