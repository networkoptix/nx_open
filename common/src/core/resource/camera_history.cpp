
#include "camera_history.h"

extern "C"
{
    #include <libavutil/avutil.h>
}

#include <core/resource_management/resource_pool.h>
#include <core/resource/network_resource.h>
#include <core/resource/media_server_resource.h>
#include "utils/common/warnings.h"
#include "utils/common/util.h"


QnId QnCameraTimePeriod::getServerId() const
{
    QnId id;

    QnResourcePtr resource = qnResPool->getResourceByGuid(mediaServerGuid);
    if (resource)
        id = resource->getId();

    return id;
}

QnCameraHistory::QnCameraHistory(): m_mutex(QMutex::Recursive)
{

}


QString QnCameraHistory::getPhysicalId() const
{
    return m_physicalId;
}

void QnCameraHistory::setPhysicalId(const QString& physicalId)
{
    m_physicalId = physicalId;
}

QnCameraTimePeriodList QnCameraHistory::getOnlineTimePeriods() const
{
    QMutexLocker lock(&m_mutex);
    QnCameraTimePeriodList result;
    for (QnCameraTimePeriodList::const_iterator itr = m_fullTimePeriods.constBegin(); itr != m_fullTimePeriods.constEnd(); ++itr)
    {
        QnResourcePtr resource = qnResPool->getResourceByGuid(itr->mediaServerGuid);
        if (resource && !resource->isDisabled() && (resource->getStatus() == QnResource::Online || resource->getStatus() == QnResource::Recording))
            result << *itr;        
    }
    return result;
}

QnCameraTimePeriodList::const_iterator QnCameraHistory::getMediaServerOnTimeItr(const QnCameraTimePeriodList& timePeriods, qint64 timestamp, bool searchForward)
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

QnMediaServerResourcePtr QnCameraHistory::getMediaServerOnTime(qint64 timestamp, bool searchForward, QnTimePeriod& currentPeriod, bool gotOfflineCameras)
{
    QMutexLocker lock(&m_mutex);
    QnCameraTimePeriodList timePeriods = gotOfflineCameras ? m_fullTimePeriods : getOnlineTimePeriods();

    QnCameraTimePeriodList::const_iterator itr;
    if (timestamp == DATETIME_NOW && !timePeriods.isEmpty())
        itr = timePeriods.constEnd()-1;
    else
        itr = getMediaServerOnTimeItr(timePeriods, timestamp, searchForward);
    if (itr == timePeriods.constEnd())
        return QnMediaServerResourcePtr();
    currentPeriod = *itr;
    return qSharedPointerDynamicCast<QnMediaServerResource>(qnResPool->getResourceById(itr->getServerId()));
}

QnNetworkResourcePtr QnCameraHistory::getCameraOnTime(qint64 timestamp, bool searchForward, bool gotOfflineCameras) {
    QnTimePeriod timePeriod;

    QnMediaServerResourcePtr server = getMediaServerOnTime(timestamp, searchForward, timePeriod, gotOfflineCameras);
    if(!server)
        return QnNetworkResourcePtr();

    return qSharedPointerDynamicCast<QnNetworkResource>(qnResPool->getResourceByUniqId(m_physicalId + server->getId().toString()));
}

QnMediaServerResourcePtr QnCameraHistory::getNextMediaServerFromTime(const QnCameraTimePeriodList& timePeriods, qint64 timestamp, QnTimePeriod& currentPeriod)
{
    QnCameraTimePeriodList::const_iterator itr = getMediaServerOnTimeItr(timePeriods, timestamp, true);
    if (itr == timePeriods.constEnd())
        return QnMediaServerResourcePtr();
    ++itr;
    if (itr == timePeriods.constEnd())
        return QnMediaServerResourcePtr();
    currentPeriod = *itr;
    return qSharedPointerDynamicCast<QnMediaServerResource> (qnResPool->getResourceById(itr->getServerId()));
}

QnMediaServerResourcePtr QnCameraHistory::getPrevMediaServerFromTime(const QnCameraTimePeriodList& timePeriods, qint64 timestamp, QnTimePeriod& currentPeriod)
{
    QnCameraTimePeriodList::const_iterator itr = getMediaServerOnTimeItr(timePeriods, timestamp, false);
    if (itr == timePeriods.constEnd() || itr == timePeriods.constBegin())
        return QnMediaServerResourcePtr();
    --itr;
    currentPeriod = *itr;
    qDebug() << "switch to previous video server. prevTime=" << QDateTime::fromMSecsSinceEpoch(timestamp).toString() << "newPeriod=" <<
        QDateTime::fromMSecsSinceEpoch(currentPeriod.startTimeMs).toString() << "-" << QDateTime::fromMSecsSinceEpoch(currentPeriod.endTimeMs()).toString();
    return qSharedPointerDynamicCast<QnMediaServerResource> (qnResPool->getResourceById(itr->getServerId()));
}

QnMediaServerResourcePtr QnCameraHistory::getNextMediaServerOnTime(qint64 timestamp, bool searchForward, QnTimePeriod& currentPeriod)
{
    QMutexLocker lock(&m_mutex);
    QnCameraTimePeriodList timePeriods = getOnlineTimePeriods();
    return searchForward ? getNextMediaServerFromTime(timePeriods, timestamp, currentPeriod) : getPrevMediaServerFromTime(timePeriods, timestamp, currentPeriod);
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

QnNetworkResourceList QnCameraHistory::getCamerasWithSamePhysicalIdInternal(const QnTimePeriod& timePeriod, const QnCameraTimePeriodList cameraHistory)
{
    QSet<QnNetworkResourcePtr> rez;

    QMutexLocker lock (&m_mutex);

    QnCameraTimePeriodList::const_iterator itrStart = getMediaServerOnTimeItr(cameraHistory, timePeriod.startTimeMs, true);
    QnCameraTimePeriodList::const_iterator itrEnd = getMediaServerOnTimeItr(cameraHistory, timePeriod.endTimeMs(), false);
    if (itrEnd != cameraHistory.constEnd())
        itrEnd++;

    for (QnCameraTimePeriodList::const_iterator itr = itrStart; itr < itrEnd; ++itr)
    {
        QString vServerId = itr->getServerId().toString();
        QnNetworkResourcePtr camera = qSharedPointerDynamicCast<QnNetworkResource> (qnResPool->getResourceByUniqId(m_physicalId + vServerId));
        if (camera)
            rez.insert(camera);
    }

    return rez.toList();
}

QnNetworkResourceList QnCameraHistory::getAllCamerasWithSamePhysicalId(const QnTimePeriod& timePeriod)
{
    return getCamerasWithSamePhysicalIdInternal(timePeriod, m_fullTimePeriods);
}

QnNetworkResourceList QnCameraHistory::getOnlineCamerasWithSamePhysicalId(const QnTimePeriod& timePeriod)
{
    return getCamerasWithSamePhysicalIdInternal(timePeriod, getOnlineTimePeriods());
}

QnNetworkResourceList QnCameraHistory::getAllCamerasWithSamePhysicalId() {
    return getAllCamerasWithSamePhysicalId(QnTimePeriod(getMinTime(), -1));
}


// ------------------- CameraHistory Pool ------------------------

QnCameraHistoryPool::QnCameraHistoryPool(QObject *parent):
    QObject(parent),
    m_mutex(QMutex::Recursive)
{}

QnCameraHistoryPool::~QnCameraHistoryPool() {
    return;
}

Q_GLOBAL_STATIC(QnCameraHistoryPool, QnCameraHistoryPool_instance);

QnCameraHistoryPool* QnCameraHistoryPool::instance()
{
    return QnCameraHistoryPool_instance();
}

QnCameraHistoryPtr QnCameraHistoryPool::getCameraHistory(const QString& physicalId)
{
    QMutexLocker lock(&m_mutex);
    return m_cameraHistory.value(physicalId);
}

QnNetworkResourceList QnCameraHistoryPool::getAllCamerasWithSamePhysicalId(const QnNetworkResourcePtr &camera) 
{
    QnCameraHistoryPtr history = getCameraHistory(camera->getPhysicalId());
    if (!history)
        return QList<QnNetworkResourcePtr>() << camera;
    return history->getAllCamerasWithSamePhysicalId();
}

QnNetworkResourceList QnCameraHistoryPool::getAllCamerasWithSamePhysicalId(const QnNetworkResourcePtr &camera, const QnTimePeriod& timePeriod)
{
    QnCameraHistoryPtr history = getCameraHistory(camera->getPhysicalId());
    if (!history)
        return QList<QnNetworkResourcePtr>() << camera;
    return history->getAllCamerasWithSamePhysicalId(timePeriod);
}

QnNetworkResourceList QnCameraHistoryPool::getOnlineCamerasWithSamePhysicalId(const QnNetworkResourcePtr &camera, const QnTimePeriod& timePeriod)
{
    QnCameraHistoryPtr history = getCameraHistory(camera->getPhysicalId());
    if (!history)
        return QList<QnNetworkResourcePtr>() << camera;
    return history->getOnlineCamerasWithSamePhysicalId(timePeriod);
}

qint64 QnCameraHistoryPool::getMinTime(QnNetworkResourcePtr camera)
{
    if (!camera)
        return AV_NOPTS_VALUE;
    QnCameraHistoryPtr history = getCameraHistory(camera->getPhysicalId());
    if (!history)
        return AV_NOPTS_VALUE;

    return history->getMinTime();
}

void QnCameraHistoryPool::addCameraHistory(QnCameraHistoryPtr history)
{
    QnCameraHistoryPtr oldHistory, newHistory;

    {
        QMutexLocker lock(&m_mutex);
        
        QString key = history->getPhysicalId();
        
        oldHistory = m_cameraHistory.value(key);
        newHistory = history;

        m_cameraHistory[key] = history;
    }

    if(!oldHistory || (oldHistory->getCameraOnTime(DATETIME_NOW, true, true) != newHistory->getCameraOnTime(DATETIME_NOW, true, true)))
        foreach(const QnNetworkResourcePtr &camera, newHistory->getAllCamerasWithSamePhysicalId())
            emit currentCameraChanged(camera);
}

void QnCameraHistoryPool::addCameraHistoryItem(const QnCameraHistoryItem &historyItem)
{
    QnNetworkResourcePtr oldCurrentCamera, newCurrentCamera;
    QnCameraHistoryPtr cameraHistory;

    {
        QMutexLocker lock(&m_mutex);

        CameraHistoryMap::const_iterator iter = m_cameraHistory.find(historyItem.physicalId);

        if (iter != m_cameraHistory.constEnd()) {
            cameraHistory = iter.value();

            oldCurrentCamera = cameraHistory->getCameraOnTime(DATETIME_NOW, true, true);
        } else {
            cameraHistory = QnCameraHistoryPtr(new QnCameraHistory());
            cameraHistory->setPhysicalId(historyItem.physicalId);

            oldCurrentCamera = QnNetworkResourcePtr();
        }
        QnCameraTimePeriod timePeriod(historyItem.timestamp, -1, historyItem.mediaServerGuid);
        cameraHistory->addTimePeriod(timePeriod);

        m_cameraHistory[historyItem.physicalId] = cameraHistory;
        
        newCurrentCamera = cameraHistory->getCameraOnTime(DATETIME_NOW, true, true);
    }

    if(oldCurrentCamera != newCurrentCamera)
        foreach(const QnNetworkResourcePtr &camera, cameraHistory->getAllCamerasWithSamePhysicalId())
            emit currentCameraChanged(camera);
}

