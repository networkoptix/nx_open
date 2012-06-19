#include "camera_history.h"
#include "core/resourcemanagment/resource_pool.h"
#include "video_server.h"
#include "utils/common/warnings.h"

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
    QMutexLocker lock(&m_mutex);
    QnCameraTimePeriodList::const_iterator itr;
    if (timestamp == DATETIME_NOW && !m_timePeriods.isEmpty())
        itr = m_timePeriods.end()-1;
    else
        itr = getVideoServerOnTimeItr(timestamp, searchForward);
    if (itr == m_timePeriods.end())
        return QnVideoServerResourcePtr();
    currentPeriod = *itr;
    return qSharedPointerDynamicCast<QnVideoServerResource>(qnResPool->getResourceById(itr->getServerId()));
}

QnNetworkResourcePtr QnCameraHistory::getCameraOnTime(qint64 timestamp, bool searchForward) {
    QnTimePeriod timePeriod;

    QnVideoServerResourcePtr server = getVideoServerOnTime(timestamp, searchForward, timePeriod);
    if(!server)
        return QnNetworkResourcePtr();

    return qSharedPointerDynamicCast<QnNetworkResource>(qnResPool->getResourceByUniqId(m_macAddress + server->getId().toString()));
}

QnVideoServerResourcePtr QnCameraHistory::getNextVideoServerFromTime(qint64 timestamp, QnTimePeriod& currentPeriod)
{
    QMutexLocker lock(&m_mutex);
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
    QMutexLocker lock(&m_mutex);
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
    QMutexLocker lock(&m_mutex);

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
    QMutexLocker lock(&m_mutex);
    if (m_timePeriods.isEmpty())
        return AV_NOPTS_VALUE;
    return m_timePeriods.first().startTimeMs;
}

QnNetworkResourceList QnCameraHistory::getAllCamerasWithSameMac(const QnTimePeriod& timePeriod)
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

QnNetworkResourceList QnCameraHistory::getAllCamerasWithSameMac() {
    return getAllCamerasWithSameMac(QnTimePeriod(getMinTime(), -1));
}


// ------------------- CameraHistory Pool ------------------------

QnCameraHistoryPool::QnCameraHistoryPool(QObject *parent):
    QObject(parent),
    m_mutex(QMutex::Recursive)
{}

QnCameraHistoryPool::~QnCameraHistoryPool() {
    return;
}

Q_GLOBAL_STATIC(QnCameraHistoryPool, inst);

QnCameraHistoryPool* QnCameraHistoryPool::instance()
{
    return inst();
}

QnCameraHistoryPtr QnCameraHistoryPool::getCameraHistory(const QString& mac)
{
    QMutexLocker lock(&m_mutex);
    return m_cameraHistory.value(mac);
}

QnNetworkResourceList QnCameraHistoryPool::getAllCamerasWithSameMac(const QnNetworkResourcePtr &camera, const QnTimePeriod& timePeriod)
{
    QnCameraHistoryPtr history = getCameraHistory(camera->getMAC());
    if (!history)
        return QList<QnNetworkResourcePtr>() << camera;
    return history->getAllCamerasWithSameMac(timePeriod);
}

qint64 QnCameraHistoryPool::getMinTime(QnNetworkResourcePtr camera)
{
    if (!camera)
        return AV_NOPTS_VALUE;
    QnCameraHistoryPtr history = getCameraHistory(camera->getMAC());
    if (!history)
        return AV_NOPTS_VALUE;

    return history->getMinTime();
}

QnResourcePtr QnCameraHistoryPool::getCurrentCamera(const QnResourcePtr &resource) {
    QnNetworkResourcePtr camera = resource.dynamicCast<QnNetworkResource>();
    if(camera) {
        return getCurrentCamera(camera);
    } else {
        return resource;
    }
}

QnNetworkResourcePtr QnCameraHistoryPool::getCurrentCamera(const QnNetworkResourcePtr &camera) {
    if(!camera)
        return camera;

    QnCameraHistoryPtr history = getCameraHistory(camera->getMAC());
    if(!history)
        return camera;

    QnNetworkResourcePtr result = history->getCameraOnTime(DATETIME_NOW, true);
    if(!result)
        return camera;
    
    return result;
}

void QnCameraHistoryPool::addCameraHistory(QnCameraHistoryPtr history)
{
    QnCameraHistoryPtr oldHistory, newHistory;

    {
        QMutexLocker lock(&m_mutex);
        
        QString key = history->getMacAddress();
        
        oldHistory = m_cameraHistory.value(key);
        newHistory = history;

        m_cameraHistory[key] = history;
    }

    if(!oldHistory || (oldHistory->getCameraOnTime(DATETIME_NOW, true) != newHistory->getCameraOnTime(DATETIME_NOW, true)))
        foreach(const QnNetworkResourcePtr &camera, newHistory->getAllCamerasWithSameMac())
            emit currentCameraChanged(camera);
}

void QnCameraHistoryPool::addCameraHistoryItem(const QnCameraHistoryItem &historyItem)
{
    QnNetworkResourcePtr oldCurrentCamera, newCurrentCamera;
    QnCameraHistoryPtr cameraHistory;

    {
        QMutexLocker lock(&m_mutex);

        CameraHistoryMap::const_iterator iter = m_cameraHistory.find(historyItem.mac);

        if (iter != m_cameraHistory.end()) {
            cameraHistory = iter.value();

            oldCurrentCamera = cameraHistory->getCameraOnTime(DATETIME_NOW, true);
        } else {
            cameraHistory = QnCameraHistoryPtr(new QnCameraHistory());
            cameraHistory->setMacAddress(historyItem.mac);

            oldCurrentCamera = QnNetworkResourcePtr();
        }
        QnCameraTimePeriod timePeriod(historyItem.timestamp, -1, historyItem.videoServerGuid);
        cameraHistory->addTimePeriod(timePeriod);

        m_cameraHistory[historyItem.mac] = cameraHistory;
        
        newCurrentCamera = cameraHistory->getCameraOnTime(DATETIME_NOW, true);
    }

    if(oldCurrentCamera != newCurrentCamera)
        foreach(const QnNetworkResourcePtr &camera, cameraHistory->getAllCamerasWithSameMac())
            emit currentCameraChanged(camera);
}

