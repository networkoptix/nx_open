#include "camera_history.h"

extern "C"
{
    #include <libavutil/avutil.h>
}

#include <utils/common/util.h>

#include <core/resource_management/resource_pool.h>
#include <core/resource/network_resource.h>
#include <core/resource/media_server_resource.h>

QnCameraHistory::QnCameraHistory():
    m_mutex(QMutex::Recursive)
{
}

QString QnCameraHistory::getCameraUniqueId() const {
    return m_cameraUniqueId;
}

void QnCameraHistory::setCameraUniqueId(const QString &cameraUniqueId) {
    m_cameraUniqueId = cameraUniqueId;
}

QnCameraTimePeriodList QnCameraHistory::getOnlineTimePeriods() const
{
    QMutexLocker lock(&m_mutex);
    QnCameraTimePeriodList result;
    for (QnCameraTimePeriodList::const_iterator itr = m_fullTimePeriods.constBegin(); itr != m_fullTimePeriods.constEnd(); ++itr)
    {
        QnResourcePtr resource = qnResPool->getResourceById(itr->mediaServerGuid);
        if (resource && resource->getStatus() == Qn::Online)
            result.insert(itr.key(), itr.value());        
    }
    return result;
}

QnCameraTimePeriodList::const_iterator QnCameraHistory::getMediaServerOnTimeItr(const QnCameraTimePeriodList& timePeriods, qint64 timestamp, bool searchForward) const
{

    if (timePeriods.isEmpty())
        return timePeriods.constEnd();
    QnCameraTimePeriodList::const_iterator itr = timePeriods.upperBound(timestamp);
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
    return qSharedPointerDynamicCast<QnMediaServerResource>(qnResPool->getResourceById(itr->mediaServerGuid));
}

QnMediaServerResourcePtr QnCameraHistory::getLastMediaServer() const
{
    QMutexLocker lock(&m_mutex);
    if (m_fullTimePeriods.isEmpty())
        return QnMediaServerResourcePtr();
    QUuid serverId = m_fullTimePeriods.last().mediaServerGuid;
    return qSharedPointerDynamicCast<QnMediaServerResource>(qnResPool->getResourceById(serverId));
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
    return qSharedPointerDynamicCast<QnMediaServerResource>(qnResPool->getResourceById(itr->mediaServerGuid));
}

QnMediaServerResourcePtr QnCameraHistory::getNextMediaServerFromTime(const QnCameraTimePeriodList& timePeriods, qint64 timestamp) const
{
    QnCameraTimePeriodList::const_iterator itr = getMediaServerOnTimeItr(timePeriods, timestamp, true);
    if (itr == timePeriods.constEnd())
        return QnMediaServerResourcePtr();
    ++itr;
    if (itr == timePeriods.constEnd())
        return QnMediaServerResourcePtr();
    return qSharedPointerDynamicCast<QnMediaServerResource> (qnResPool->getResourceById(itr->mediaServerGuid));
}

QnMediaServerResourcePtr QnCameraHistory::getPrevMediaServerFromTime(const QnCameraTimePeriodList& timePeriods, qint64 timestamp) const
{
    QnCameraTimePeriodList::const_iterator itr = getMediaServerOnTimeItr(timePeriods, timestamp, false);
    if (itr == timePeriods.constEnd() || itr == timePeriods.constBegin())
        return QnMediaServerResourcePtr();
    --itr;
    return qSharedPointerDynamicCast<QnMediaServerResource> (qnResPool->getResourceById(itr->mediaServerGuid));
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
    return qSharedPointerDynamicCast<QnMediaServerResource> (qnResPool->getResourceById(itr->mediaServerGuid));
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
    return qSharedPointerDynamicCast<QnMediaServerResource> (qnResPool->getResourceById(itr->mediaServerGuid));
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

    m_fullTimePeriods.insert(period.startTimeMs, period);
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

    for (QnCameraTimePeriodList::const_iterator itr = itrStart; itr != itrEnd; ++itr)
    {
        QnMediaServerResourcePtr mServer = qnResPool->getResourceById(itr->mediaServerGuid).dynamicCast<QnMediaServerResource>();
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

QnCameraHistoryPtr QnCameraHistoryPool::getCameraHistory(const QnResourcePtr& camera) const {
    QMutexLocker lock(&m_mutex);
    return m_cameraHistory.value(camera->getUniqueId());
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
    QnCameraHistoryPtr history = getCameraHistory(camera);
    return history ? history->getAllCameraServers() : getCurrentServer(camera);
}

QnMediaServerResourcePtr QnCameraHistoryPool::getMediaServerOnTime(const QnNetworkResourcePtr &camera, qint64 timestamp) const
{
    QnCameraHistoryPtr history = getCameraHistory(camera);
    return history ? history->getMediaServerOnTime(timestamp, true, true) : QnMediaServerResourcePtr();
}

QnMediaServerResourceList QnCameraHistoryPool::getAllCameraServers(const QnNetworkResourcePtr &camera, const QnTimePeriod& timePeriod) const
{
    QnCameraHistoryPtr history = getCameraHistory(camera);
    return history ? history->getAllCameraServers(timePeriod) : getCurrentServer(camera);
}

QnMediaServerResourceList QnCameraHistoryPool::getOnlineCameraServers(const QnNetworkResourcePtr &camera, const QnTimePeriod& timePeriod) const
{
    QnCameraHistoryPtr history = getCameraHistory(camera);
    return history ? history->getOnlineCameraServers(timePeriod) : getCurrentServer(camera);
}

qint64 QnCameraHistoryPool::getMinTime(const QnNetworkResourcePtr &camera)
{
    if (!camera)
        return AV_NOPTS_VALUE;

    QnCameraHistoryPtr history = getCameraHistory(camera);
    if (!history)
        return AV_NOPTS_VALUE;

    return history->getMinTime();
}

void QnCameraHistoryPool::addCameraHistory(const QnCameraHistoryPtr &history)
{
    QnCameraHistoryPtr oldHistory, newHistory;
    {
        QMutexLocker lock(&m_mutex);
        
        QString key = history->getCameraUniqueId();
        
        oldHistory = m_cameraHistory.value(key);
        newHistory = history;

        m_cameraHistory[key] = history;
    }
    
    if(!oldHistory || (oldHistory->getMediaServerOnTime(DATETIME_NOW, true, true) != newHistory->getMediaServerOnTime(DATETIME_NOW, true, true))) {
        QnNetworkResourcePtr camera = qnResPool->getResourceByUniqId(history->getCameraUniqueId()).dynamicCast<QnNetworkResource>();
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

        CameraHistoryMap::const_iterator iter = m_cameraHistory.find(historyItem.cameraUniqueId);

        if (iter != m_cameraHistory.constEnd()) {
            cameraHistory = iter.value();

            oldServer = cameraHistory->getMediaServerOnTime(DATETIME_NOW, true, true);
        } else {
            cameraHistory = QnCameraHistoryPtr(new QnCameraHistory());
            cameraHistory->setCameraUniqueId(historyItem.cameraUniqueId);

            oldServer = QnResourcePtr();
        }
        QnCameraTimePeriod timePeriod(historyItem.timestamp, -1, historyItem.mediaServerGuid);
        cameraHistory->addTimePeriod(timePeriod);

        m_cameraHistory[historyItem.cameraUniqueId] = cameraHistory;
        
        newServer = cameraHistory->getMediaServerOnTime(DATETIME_NOW, true, true);
    }

    if(oldServer != newServer) {
        QnNetworkResourcePtr camera = qnResPool->getResourceByUniqId(historyItem.cameraUniqueId).dynamicCast<QnNetworkResource>();
        if (camera)
            emit currentCameraChanged(camera);
    }
}

