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

QnServerHistoryMap QnCameraHistory::getOnlineTimePeriods() const
{
    QMutexLocker lock(&m_mutex);
    QnServerHistoryMap result;
    for (QnServerHistoryMap::const_iterator itr = m_fullTimePeriods.constBegin(); itr != m_fullTimePeriods.constEnd(); ++itr)
    {
        QnResourcePtr resource = qnResPool->getResourceById(itr.value());
        if (resource && resource->getStatus() == Qn::Online)
            result.insert(itr.key(), itr.value());        
    }
    return result;
}

QnServerHistoryMap::const_iterator QnCameraHistory::getMediaServerOnTimeItr(const QnServerHistoryMap& timePeriods, qint64 timestamp) const
{
    if (timePeriods.isEmpty())
        return timePeriods.constEnd();
    QnServerHistoryMap::const_iterator itr = timePeriods.upperBound(timestamp);
    if (itr != timePeriods.constBegin())
        --itr;
    return itr;
}

QnMediaServerResourcePtr QnCameraHistory::getMediaServerOnTime(qint64 timestamp, bool allowOfflineServers) const
{
    QMutexLocker lock(&m_mutex);
    QnServerHistoryMap timePeriods = allowOfflineServers ? m_fullTimePeriods : getOnlineTimePeriods();

    QnServerHistoryMap::const_iterator itr;
    if (timestamp == DATETIME_NOW && !timePeriods.isEmpty())
        itr = timePeriods.constEnd()-1;
    else
        itr = getMediaServerOnTimeItr(timePeriods, timestamp);
    if (itr == timePeriods.constEnd())
        return QnMediaServerResourcePtr();
    return qSharedPointerDynamicCast<QnMediaServerResource>(qnResPool->getResourceById(itr.value()));
}

QnMediaServerResourcePtr QnCameraHistory::getMediaServerAndPeriodOnTime(qint64 timestamp, QnTimePeriod& currentPeriod, bool allowOfflineServers)
{
    QMutexLocker lock(&m_mutex);
    QnServerHistoryMap timePeriods = allowOfflineServers ? m_fullTimePeriods : getOnlineTimePeriods();

    QnServerHistoryMap::const_iterator itr;
    if (timestamp == DATETIME_NOW && !timePeriods.isEmpty())
        itr = timePeriods.constEnd()-1;
    else
        itr = getMediaServerOnTimeItr(timePeriods, timestamp);
    if (itr == timePeriods.constEnd())
        return QnMediaServerResourcePtr();
    
    currentPeriod.startTimeMs = itr.key();
    currentPeriod.durationMs = -1;
    QnMediaServerResourcePtr result = qSharedPointerDynamicCast<QnMediaServerResource>(qnResPool->getResourceById(itr.value()));
    ++itr;
    if (itr != timePeriods.constEnd())
        currentPeriod.durationMs = itr.key() - currentPeriod.startTimeMs;

    return result;
}

QnMediaServerResourcePtr QnCameraHistory::getNextMediaServerFromTime(const QnServerHistoryMap& timePeriods, qint64 timestamp) const
{
    QnServerHistoryMap::const_iterator itr = getMediaServerOnTimeItr(timePeriods, timestamp);
    if (itr == timePeriods.constEnd())
        return QnMediaServerResourcePtr();
    ++itr;
    if (itr == timePeriods.constEnd())
        return QnMediaServerResourcePtr();
    return qSharedPointerDynamicCast<QnMediaServerResource> (qnResPool->getResourceById(itr.value()));
}

QnMediaServerResourcePtr QnCameraHistory::getPrevMediaServerFromTime(const QnServerHistoryMap& timePeriods, qint64 timestamp) const
{
    QnServerHistoryMap::const_iterator itr = getMediaServerOnTimeItr(timePeriods, timestamp);
    if (itr == timePeriods.constEnd() || itr == timePeriods.constBegin())
        return QnMediaServerResourcePtr();
    --itr;
    return qSharedPointerDynamicCast<QnMediaServerResource> (qnResPool->getResourceById(itr.value()));
}

QnMediaServerResourcePtr QnCameraHistory::getNextMediaServerAndPeriodFromTime(const QnServerHistoryMap& timePeriods, qint64 timestamp, QnTimePeriod& currentPeriod)
{
    QnServerHistoryMap::const_iterator itr = getMediaServerOnTimeItr(timePeriods, timestamp);
    if (itr == timePeriods.constEnd())
        return QnMediaServerResourcePtr();
    ++itr;
    if (itr == timePeriods.constEnd())
        return QnMediaServerResourcePtr();
    currentPeriod.startTimeMs = itr.key();
    currentPeriod.durationMs = -1;
    QnMediaServerResourcePtr result = qSharedPointerDynamicCast<QnMediaServerResource> (qnResPool->getResourceById(itr.value()));
    ++itr;
    if (itr != timePeriods.constEnd())
        currentPeriod.durationMs = itr.key() - currentPeriod.startTimeMs;
    return result;
}

QnMediaServerResourcePtr QnCameraHistory::getPrevMediaServerAndPeriodFromTime(const QnServerHistoryMap& timePeriods, qint64 timestamp, QnTimePeriod& currentPeriod)
{
    QnServerHistoryMap::const_iterator itr = getMediaServerOnTimeItr(timePeriods, timestamp);
    if (itr == timePeriods.constEnd() || itr == timePeriods.constBegin())
        return QnMediaServerResourcePtr();
    qint64 nextStartTime = itr.key();
    --itr;
    currentPeriod.startTimeMs = itr.key();
    currentPeriod.durationMs = nextStartTime - currentPeriod.startTimeMs;
    qDebug() << "switch to previous video server. prevTime=" << QDateTime::fromMSecsSinceEpoch(timestamp).toString() << "newPeriod=" <<
        QDateTime::fromMSecsSinceEpoch(currentPeriod.startTimeMs).toString() << "-" << QDateTime::fromMSecsSinceEpoch(currentPeriod.endTimeMs()).toString();
    return qSharedPointerDynamicCast<QnMediaServerResource> (qnResPool->getResourceById(itr.value()));
}

QnMediaServerResourcePtr QnCameraHistory::getNextMediaServerOnTime(qint64 timestamp, bool searchForward) const
{
    QMutexLocker lock(&m_mutex);
    QnServerHistoryMap timePeriods = getOnlineTimePeriods();
    return searchForward ? getNextMediaServerFromTime(timePeriods, timestamp) : getPrevMediaServerFromTime(timePeriods, timestamp);
}

QnMediaServerResourcePtr QnCameraHistory::getNextMediaServerAndPeriodOnTime(qint64 timestamp, QnTimePeriod& currentPeriod, bool searchForward)
{
    QMutexLocker lock(&m_mutex);
    QnServerHistoryMap timePeriods = getOnlineTimePeriods();
    return searchForward ? getNextMediaServerAndPeriodFromTime(timePeriods, timestamp, currentPeriod) : getPrevMediaServerAndPeriodFromTime(timePeriods, timestamp, currentPeriod);
}

void QnCameraHistory::addTimePeriod(qint64 timestamp, const QUuid& serverId)
{
    QMutexLocker lock(&m_mutex);
    m_fullTimePeriods.insert(timestamp, serverId);
}

void QnCameraHistory::removeTimePeriod(const qint64 timestamp)
{
    QMutexLocker lock(&m_mutex);
    m_fullTimePeriods.remove(timestamp);
}

qint64 QnCameraHistory::getMinTime() const
{
    QMutexLocker lock(&m_mutex);
    if (m_fullTimePeriods.isEmpty())
        return AV_NOPTS_VALUE;
    return m_fullTimePeriods.begin().key();
}

QnMediaServerResourceList QnCameraHistory::getAllCameraServersInternal(const QnTimePeriod& timePeriod, const QnServerHistoryMap cameraHistory) const
{
    QSet<QnMediaServerResourcePtr> rez;

    QMutexLocker lock (&m_mutex);

    QnServerHistoryMap::const_iterator itrStart = getMediaServerOnTimeItr(cameraHistory, timePeriod.startTimeMs);
    QnServerHistoryMap::const_iterator itrEnd = getMediaServerOnTimeItr(cameraHistory, timePeriod.endTimeMs());
    if (itrEnd != cameraHistory.constEnd())
        itrEnd++;

    for (QnServerHistoryMap::const_iterator itr = itrStart; itr != itrEnd; ++itr)
    {
        QnMediaServerResourcePtr mServer = qnResPool->getResourceById(itr.value()).dynamicCast<QnMediaServerResource>();
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

void QnCameraHistory::getItemsBefore(qint64 timestamp,  const QUuid& serverId, QList<QnCameraHistoryItem>& result)
{
    QMutexLocker lock (&m_mutex);
    for (auto itr = m_fullTimePeriods.begin(); itr != m_fullTimePeriods.end() && itr.key() < timestamp; ++itr)
    {
        if (itr.value() == serverId)
            result << QnCameraHistoryItem(m_cameraUniqueId, itr.key(), serverId);
    }
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

QnCameraHistoryPtr QnCameraHistoryPool::getCameraHistory(const QString& uniqueId) const {
    QMutexLocker lock(&m_mutex);
    return m_cameraHistory.value(uniqueId);
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

QnMediaServerResourcePtr QnCameraHistoryPool::getMediaServerOnTime(const QnNetworkResourcePtr &camera, qint64 timestamp, bool allowOfflineServers) const
{
    QnCameraHistoryPtr history = getCameraHistory(camera);
    return history ? history->getMediaServerOnTime(timestamp, allowOfflineServers) : QnMediaServerResourcePtr();
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
    QMutexLocker lock(&m_mutex);
    m_cameraHistory[history->getCameraUniqueId()] = history;
}

void QnCameraHistoryPool::addCameraHistoryItem(const QnCameraHistoryItem &historyItem)
{
    QMutexLocker lock(&m_mutex);

    CameraHistoryMap::const_iterator iter = m_cameraHistory.find(historyItem.cameraUniqueId);
    QnCameraHistoryPtr cameraHistory;
    if (iter != m_cameraHistory.constEnd()) {
        cameraHistory = iter.value();

    } else {
        cameraHistory = QnCameraHistoryPtr(new QnCameraHistory());
        cameraHistory->setCameraUniqueId(historyItem.cameraUniqueId);
        m_cameraHistory.insert(cameraHistory->getCameraUniqueId(), cameraHistory);
    }
    cameraHistory->addTimePeriod(historyItem.timestamp, historyItem.mediaServerGuid);
}

void QnCameraHistoryPool::removeCameraHistoryItem(const QnCameraHistoryItem& historyItem)
{
    QnCameraHistoryPtr history = getCameraHistory(historyItem.cameraUniqueId);
    if (history) 
        history->removeTimePeriod(historyItem.timestamp);
}

QList<QnCameraHistoryItem> QnCameraHistoryPool::getUnusedItems(const QMap<QString, qint64>& archiveMinTimes, const QUuid& serverId)
{
    QMutexLocker lock(&m_mutex);

    QList<QnCameraHistoryItem> result;
    for(auto itr = archiveMinTimes.begin(); itr != archiveMinTimes.end(); ++itr)
    {
        auto historyItr = m_cameraHistory.find(itr.key());
        if (historyItr != m_cameraHistory.end())
            historyItr.value()->getItemsBefore(itr.value(), serverId, result);
    }
    return result;
}
