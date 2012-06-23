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
    QMutexLocker lock(&m_mutex);
    return getOnlineTimePeriods();
}

QnCameraTimePeriodList QnCameraHistory::getOnlineTimePeriods() const
{
    QnCameraTimePeriodList result;
    for (QnCameraTimePeriodList::const_iterator itr = m_fullTimePeriods.constBegin(); itr != m_fullTimePeriods.constEnd(); ++itr)
    {
        QnResourcePtr resource = qnResPool->getResourceByGuid(itr->videoServerGuid);
        if (resource && !resource->isDisabled() && (resource->getStatus() == QnResource::Online || resource->getStatus() == QnResource::Recording))
            result << *itr;        
    }
    return result;
}

QnCameraTimePeriodList::const_iterator QnCameraHistory::getVideoServerOnTimeItr(const QnCameraTimePeriodList& timePeriods, qint64 timestamp, bool searchForward)
{

    if (timePeriods.isEmpty())
        return timePeriods.constEnd();
    QnCameraTimePeriodList::const_iterator itr = qUpperBound(timePeriods.constBegin(), timePeriods.constEnd(), timestamp);
    if (itr != timePeriods.constBegin())
        --itr;
    if (searchForward && timestamp > itr->endTimeMs())
        ++itr;
    return itr;
}

QnVideoServerResourcePtr QnCameraHistory::getVideoServerOnTime(qint64 timestamp, bool searchForward, QnTimePeriod& currentPeriod, bool gotOfflineCameras)
{
    QMutexLocker lock(&m_mutex);
    QnCameraTimePeriodList timePeriods = gotOfflineCameras ? m_fullTimePeriods : getOnlineTimePeriods();

    QnCameraTimePeriodList::const_iterator itr;
    if (timestamp == DATETIME_NOW && !timePeriods.isEmpty())
        itr = timePeriods.constEnd()-1;
    else
        itr = getVideoServerOnTimeItr(timePeriods, timestamp, searchForward);
    if (itr == timePeriods.constEnd())
        return QnVideoServerResourcePtr();
    currentPeriod = *itr;
    return qSharedPointerDynamicCast<QnVideoServerResource>(qnResPool->getResourceById(itr->getServerId()));
}

QnNetworkResourcePtr QnCameraHistory::getCameraOnTime(qint64 timestamp, bool searchForward, bool gotOfflineCameras) {
    QnTimePeriod timePeriod;

    QnVideoServerResourcePtr server = getVideoServerOnTime(timestamp, searchForward, timePeriod, gotOfflineCameras);
    if(!server)
        return QnNetworkResourcePtr();

    return qSharedPointerDynamicCast<QnNetworkResource>(qnResPool->getResourceByUniqId(m_macAddress + server->getId().toString()));
}

QnVideoServerResourcePtr QnCameraHistory::getNextVideoServerFromTime(const QnCameraTimePeriodList& timePeriods, qint64 timestamp, QnTimePeriod& currentPeriod)
{
    QnCameraTimePeriodList::const_iterator itr = getVideoServerOnTimeItr(timePeriods, timestamp, true);
    if (itr == timePeriods.constEnd())
        return QnVideoServerResourcePtr();
    ++itr;
    if (itr == timePeriods.constEnd())
        return QnVideoServerResourcePtr();
    currentPeriod = *itr;
    return qSharedPointerDynamicCast<QnVideoServerResource> (qnResPool->getResourceById(itr->getServerId()));
}

QnVideoServerResourcePtr QnCameraHistory::getPrevVideoServerFromTime(const QnCameraTimePeriodList& timePeriods, qint64 timestamp, QnTimePeriod& currentPeriod)
{
    QnCameraTimePeriodList::const_iterator itr = getVideoServerOnTimeItr(timePeriods, timestamp, false);
    if (itr == timePeriods.constEnd() || itr == timePeriods.constBegin())
        return QnVideoServerResourcePtr();
    --itr;
    currentPeriod = *itr;
	qDebug() << "switch to previous video server. prevTime=" << QDateTime::fromMSecsSinceEpoch(timestamp).toString() << "newPeriod=" <<
		QDateTime::fromMSecsSinceEpoch(currentPeriod.startTimeMs).toString() << "-" << QDateTime::fromMSecsSinceEpoch(currentPeriod.endTimeMs()).toString();
    return qSharedPointerDynamicCast<QnVideoServerResource> (qnResPool->getResourceById(itr->getServerId()));
}

QnVideoServerResourcePtr QnCameraHistory::getNextVideoServerOnTime(qint64 timestamp, bool searchForward, QnTimePeriod& currentPeriod)
{
    QMutexLocker lock(&m_mutex);
    QnCameraTimePeriodList timePeriods = getOnlineTimePeriods();
    return searchForward ? getNextVideoServerFromTime(timePeriods, timestamp, currentPeriod) : getPrevVideoServerFromTime(timePeriods, timestamp, currentPeriod);
}

void QnCameraHistory::addTimePeriod(const QnCameraTimePeriod& period)
{
    QMutexLocker lock(&m_mutex);

    // Works only if "period" startTimeMs is > the last item startTimeMs
    if (!m_fullTimePeriods.isEmpty())
    {
        QnTimePeriod& lastItem = m_fullTimePeriods.last();

        if (lastItem.durationMs == -1)
            lastItem.durationMs = period.startTimeMs - lastItem.startTimeMs;

    }

    m_fullTimePeriods << period;
    //qSort(m_timePeriods.begin(), m_timePeriods.end());
}

qint64 QnCameraHistory::getMinTime() const
{
    QMutexLocker lock(&m_mutex);
    if (m_fullTimePeriods.isEmpty())
        return AV_NOPTS_VALUE;
    return m_fullTimePeriods.first().startTimeMs;
}

QnNetworkResourceList QnCameraHistory::getAllCamerasWithSameMac(const QnTimePeriod& timePeriod)
{
    QSet<QnNetworkResourcePtr> rez;

    QMutexLocker lock (&m_mutex);

    QnCameraTimePeriodList::const_iterator itrStart = getVideoServerOnTimeItr(m_fullTimePeriods, timePeriod.startTimeMs, true);
    QnCameraTimePeriodList::const_iterator itrEnd = getVideoServerOnTimeItr(m_fullTimePeriods, timePeriod.endTimeMs(), false);
    if (itrEnd != m_fullTimePeriods.constEnd())
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

/*
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

    QnCameraHistoryPtr history = getCameraHistory(camera->getMAC().toString());
    if(!history)
        return camera;

    QnNetworkResourcePtr result = history->getCameraOnTime(DATETIME_NOW, true);
    if(!result)
        return camera;
    
    return result;
}
*/

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

    if(!oldHistory || (oldHistory->getCameraOnTime(DATETIME_NOW, true, true) != newHistory->getCameraOnTime(DATETIME_NOW, true, true)))
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

        if (iter != m_cameraHistory.constEnd()) {
            cameraHistory = iter.value();

            oldCurrentCamera = cameraHistory->getCameraOnTime(DATETIME_NOW, true, true);
        } else {
            cameraHistory = QnCameraHistoryPtr(new QnCameraHistory());
            cameraHistory->setMacAddress(historyItem.mac);

            oldCurrentCamera = QnNetworkResourcePtr();
        }
        QnCameraTimePeriod timePeriod(historyItem.timestamp, -1, historyItem.videoServerGuid);
        cameraHistory->addTimePeriod(timePeriod);

        m_cameraHistory[historyItem.mac] = cameraHistory;
        
        newCurrentCamera = cameraHistory->getCameraOnTime(DATETIME_NOW, true, true);
    }

    if(oldCurrentCamera != newCurrentCamera)
        foreach(const QnNetworkResourcePtr &camera, cameraHistory->getAllCamerasWithSameMac())
            emit currentCameraChanged(camera);
}

