#include "camera_history.h"

extern "C"
{
    #include <libavutil/avutil.h>
}

#include <utils/common/util.h>

#include <core/resource_management/resource_pool.h>
#include <core/resource/network_resource.h>
#include <core/resource/media_server_resource.h>


QnId QnCameraTimePeriod::getServerId() const
{
    QnId id;

    QnResourcePtr resource = qnResPool->getResourceById(mediaServerGuid);
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
        QnResourcePtr resource = qnResPool->getResourceById(itr->mediaServerGuid);
        if (resource && resource->getStatus() == QnResource::Online)
            result << *itr;        
    }
    return result;
}

QnCameraTimePeriodList::const_iterator QnCameraHistory::getMediaServerOnTimeItr(const QnCameraTimePeriodList& timePeriods, qint64 timestamp, bool searchForward) const
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

QnMediaServerResourcePtr QnCameraHistory::getMediaServerOnTime(qint64 timestamp, bool searchForward, bool allowOfflineServers) const
{
    QMutexLocker lock(&m_mutex);
    QnCameraTimePeriodList timePeriods = allowOfflineServers ? m_fullTimePeriods : getOnlineTimePeriods();

    QnCameraTimePeriodList::const_iterator itr;
    if (timestamp == DATETIME_NOW && !timePeriods.isEmpty())
        itr = timePeriods.constEnd()-1;
    else
        itr = getMediaServerOnTimeItr(timePeriods, timestamp, searchForward);
    if (itr == timePeriods.constEnd())
        return QnMediaServerResourcePtr();
    return qSharedPointerDynamicCast<QnMediaServerResource>(qnResPool->getResourceById(itr->getServerId()));
}

QnMediaServerResourcePtr QnCameraHistory::getMediaServerAndPeriodOnTime(qint64 timestamp, bool searchForward, QnTimePeriod& currentPeriod, bool allowOfflineServers)
{
    QMutexLocker lock(&m_mutex);
    QnCameraTimePeriodList timePeriods = allowOfflineServers ? m_fullTimePeriods : getOnlineTimePeriods();

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

QnMediaServerResourcePtr QnCameraHistory::getNextMediaServerFromTime(const QnCameraTimePeriodList& timePeriods, qint64 timestamp) const
{
    QnCameraTimePeriodList::const_iterator itr = getMediaServerOnTimeItr(timePeriods, timestamp, true);
    if (itr == timePeriods.constEnd())
        return QnMediaServerResourcePtr();
    ++itr;
    if (itr == timePeriods.constEnd())
        return QnMediaServerResourcePtr();
    return qSharedPointerDynamicCast<QnMediaServerResource> (qnResPool->getResourceById(itr->getServerId()));
}

QnMediaServerResourcePtr QnCameraHistory::getPrevMediaServerFromTime(const QnCameraTimePeriodList& timePeriods, qint64 timestamp) const
{
    QnCameraTimePeriodList::const_iterator itr = getMediaServerOnTimeItr(timePeriods, timestamp, false);
    if (itr == timePeriods.constEnd() || itr == timePeriods.constBegin())
        return QnMediaServerResourcePtr();
    --itr;
    return qSharedPointerDynamicCast<QnMediaServerResource> (qnResPool->getResourceById(itr->getServerId()));
}

QnMediaServerResourcePtr QnCameraHistory::getNextMediaServerAndPeriodFromTime(const QnCameraTimePeriodList& timePeriods, qint64 timestamp, QnTimePeriod& currentPeriod)
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

QnMediaServerResourcePtr QnCameraHistory::getPrevMediaServerAndPeriodFromTime(const QnCameraTimePeriodList& timePeriods, qint64 timestamp, QnTimePeriod& currentPeriod)
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


QnMediaServerResourcePtr QnCameraHistory::getNextMediaServerOnTime(qint64 timestamp, bool searchForward) const
{
    QMutexLocker lock(&m_mutex);
    QnCameraTimePeriodList timePeriods = getOnlineTimePeriods();
    return searchForward ? getNextMediaServerFromTime(timePeriods, timestamp) : getPrevMediaServerFromTime(timePeriods, timestamp);
}

QnMediaServerResourcePtr QnCameraHistory::getNextMediaServerAndPeriodOnTime(qint64 timestamp, bool searchForward, QnTimePeriod& currentPeriod)
{
    QMutexLocker lock(&m_mutex);
    QnCameraTimePeriodList timePeriods = getOnlineTimePeriods();
    return searchForward ? getNextMediaServerAndPeriodFromTime(timePeriods, timestamp, currentPeriod) : getPrevMediaServerAndPeriodFromTime(timePeriods, timestamp, currentPeriod);
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

QnMediaServerResourceList QnCameraHistory::getAllCameraServersInternal(const QnTimePeriod& timePeriod, const QnCameraTimePeriodList cameraHistory) const
{
    QSet<QnMediaServerResourcePtr> rez;

    QMutexLocker lock (&m_mutex);

    QnCameraTimePeriodList::const_iterator itrStart = getMediaServerOnTimeItr(cameraHistory, timePeriod.startTimeMs, true);
    QnCameraTimePeriodList::const_iterator itrEnd = getMediaServerOnTimeItr(cameraHistory, timePeriod.endTimeMs(), false);
    if (itrEnd != cameraHistory.constEnd())
        itrEnd++;

    for (QnCameraTimePeriodList::const_iterator itr = itrStart; itr < itrEnd; ++itr)
    {
        QnMediaServerResourcePtr mServer = qnResPool->getResourceById(itr->getServerId()).dynamicCast<QnMediaServerResource>();
        if (mServer)
            rez.insert(mServer);
    }
    
    return rez.toList();
}

QnMediaServerResourceList QnCameraHistory::getAllCameraServers(const QnTimePeriod& timePeriod) const {
    return getAllCameraServersInternal(timePeriod, m_fullTimePeriods);
}

QnMediaServerResourceList QnCameraHistory::getOnlineCameraServers(const QnTimePeriod& timePeriod) const {
    return getAllCameraServersInternal(timePeriod, getOnlineTimePeriods());
}

QnMediaServerResourceList QnCameraHistory::getAllCameraServers() const {
    return getAllCameraServers(QnTimePeriod(getMinTime(), -1));
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

QnCameraHistoryPtr QnCameraHistoryPool::getCameraHistory(const QString& physicalId) const
{
    QMutexLocker lock(&m_mutex);
    return m_cameraHistory.value(physicalId);
}

QnMediaServerResourceList QnCameraHistoryPool::getCurrentServer(const QnNetworkResourcePtr &camera) const
{
    QnMediaServerResourceList rez;
    QnMediaServerResourcePtr server = qnResPool->getResourceById(camera->getParentId()).dynamicCast<QnMediaServerResource>();
    if (server)
        rez << server;
    return rez;
}

QnMediaServerResourceList QnCameraHistoryPool::getAllCameraServers(const QnNetworkResourcePtr &camera) const
{
    QnCameraHistoryPtr history = getCameraHistory(camera->getPhysicalId());
    return history ? history->getAllCameraServers() : getCurrentServer(camera);
}

QnMediaServerResourceList QnCameraHistoryPool::getAllCameraServers(const QnNetworkResourcePtr &camera, const QnTimePeriod& timePeriod) const
{
    QnCameraHistoryPtr history = getCameraHistory(camera->getPhysicalId());
    return history ? history->getAllCameraServers(timePeriod) : getCurrentServer(camera);
}

QnMediaServerResourceList QnCameraHistoryPool::getOnlineCameraServers(const QnNetworkResourcePtr &camera, const QnTimePeriod& timePeriod) const
{
    QnCameraHistoryPtr history = getCameraHistory(camera->getPhysicalId());
    return history ? history->getOnlineCameraServers(timePeriod) : getCurrentServer(camera);
}

qint64 QnCameraHistoryPool::getMinTime(const QnNetworkResourcePtr &camera)
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
    
    if(!oldHistory || (oldHistory->getMediaServerOnTime(DATETIME_NOW, true, true) != newHistory->getMediaServerOnTime(DATETIME_NOW, true, true))) {
        QnNetworkResourcePtr camera = qnResPool->getNetResourceByPhysicalId(history->getPhysicalId());
        if (camera)
            emit currentCameraChanged(camera);
    }
}

void QnCameraHistoryPool::addCameraHistoryItem(const QnCameraHistoryItem &historyItem)
{
    QnResourcePtr oldServer, newServer;
    QnCameraHistoryPtr cameraHistory;

    {
        QMutexLocker lock(&m_mutex);

        CameraHistoryMap::const_iterator iter = m_cameraHistory.find(historyItem.physicalId);

        if (iter != m_cameraHistory.constEnd()) {
            cameraHistory = iter.value();

            oldServer = cameraHistory->getMediaServerOnTime(DATETIME_NOW, true, true);
        } else {
            cameraHistory = QnCameraHistoryPtr(new QnCameraHistory());
            cameraHistory->setPhysicalId(historyItem.physicalId);

            oldServer = QnResourcePtr();
        }
        QnCameraTimePeriod timePeriod(historyItem.timestamp, -1, historyItem.mediaServerGuid);
        cameraHistory->addTimePeriod(timePeriod);

        m_cameraHistory[historyItem.physicalId] = cameraHistory;
        
        newServer = cameraHistory->getMediaServerOnTime(DATETIME_NOW, true, true);
    }

    if(oldServer != newServer) {
        QnNetworkResourcePtr camera = qnResPool->getNetResourceByPhysicalId(historyItem.physicalId);
        if (camera)
            emit currentCameraChanged(camera);
    }
}

