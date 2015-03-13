#include "camera_history.h"

#include <utils/common/util.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/network_resource.h>
#include <core/resource/media_server_resource.h>

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

QnMediaServerResourcePtr QnCameraHistoryPool::getCurrentServer(const QnNetworkResourcePtr &camera) const
{
    return qnResPool->getResourceById(camera->getParentId()).dynamicCast<QnMediaServerResource>();
}

QnMediaServerResourceList QnCameraHistoryPool::getServersByCamera(const QnNetworkResourcePtr &camera) const
{
    QnMediaServerResourceList result;
    QnUuid id = camera->getId();
    for(auto itr = m_archivedCameras.begin(); itr != m_archivedCameras.end(); ++itr)
    {
        const auto& data = itr.value();
        if (std::find(data.begin(), data.end(), id) != data.end())
        {
            QnMediaServerResourcePtr mServer = toMediaServer(itr.key());
            if (mServer)
                result << mServer;
        }
    }
    if (result.isEmpty())
        result << getCurrentServer(camera);
    return result;
}

QnMediaServerResourceList QnCameraHistoryPool::getServersByCamera(const QnNetworkResourcePtr &camera, const QnTimePeriod& timePeriod) const
{
    auto itr = m_historyDetail.find(camera->getId());
    if (itr == m_historyDetail.end())
        return QnMediaServerResourceList() << getCurrentServer(camera);
    const auto& moveData = itr.value();
    if (moveData.empty())
        return QnMediaServerResourceList() << getCurrentServer(camera);
    auto itr2 = getMediaServerOnTimeInternal(moveData, timePeriod.startTimeMs);
    Q_ASSERT(itr2 != moveData.end());
    QSet<QnMediaServerResourcePtr> result;
    while (itr2 != moveData.end() && itr2->timestampMs < timePeriod.endTimeMs()) {
        QnMediaServerResourcePtr mServer = toMediaServer(itr2->serverGuid);
        if (mServer)
            result << mServer;
    }
    return result.toList();
}

ec2::ApiCameraHistoryMoveDataList::const_iterator QnCameraHistoryPool::getMediaServerOnTimeInternal(const ec2::ApiCameraHistoryMoveDataList& data, qint64 timestamp) const
{
    // todo: test me!!!
    return std::lower_bound(data.begin(), data.end(), timestamp, [](const ec2::ApiCameraHistoryMoveData& data, qint64 timestamp) { 
        return timestamp < data.timestampMs; // inverse compare
    });
}

ec2::ApiCameraHistoryMoveDataList QnCameraHistoryPool::filterOnlineServers(const ec2::ApiCameraHistoryMoveDataList& dataList) const
{
    ec2::ApiCameraHistoryMoveDataList result;
    std::copy_if(dataList.cbegin(), dataList.cend(), std::back_inserter(result), [](const ec2::ApiCameraHistoryMoveData &data)
    {
        QnResourcePtr res = qnResPool->getResourceById(data.serverGuid);
        return !res || res->getStatus() != Qn::Online;
    });
    return result;
}

QnMediaServerResourcePtr QnCameraHistoryPool::toMediaServer(const QnUuid& guid) const
{
    return qnResPool->getResourceById(guid).dynamicCast<QnMediaServerResource>();
}

QnMediaServerResourcePtr QnCameraHistoryPool::getMediaServerOnTime(const QnNetworkResourcePtr &camera, qint64 timestamp, QnTimePeriod* foundPeriod) const
{
    if (foundPeriod)
        foundPeriod->clear();

    QMutexLocker lock(&m_mutex);
    const auto& itr = m_historyDetail.find(camera->getId());
    if (itr == m_historyDetail.end())
        return getCurrentServer(camera); // no history data. use current camera constantly
    const auto detailData = filterOnlineServers(itr.value());
    auto detailItr = getMediaServerOnTimeInternal(detailData, timestamp);
    if (detailItr == detailData.end())
        return QnMediaServerResourcePtr();
    QnMediaServerResourcePtr result = toMediaServer(detailItr->serverGuid);
    if (foundPeriod) {
        foundPeriod->startTimeMs = detailItr->timestampMs;
        ++detailItr;
        foundPeriod->durationMs = (detailItr == detailData.end() ? QnTimePeriod::UnlimitedPeriod : detailItr->timestampMs - foundPeriod->startTimeMs);
    }
    return result;
}

QnMediaServerResourcePtr QnCameraHistoryPool::getNextMediaServerAndPeriodOnTime(const QnNetworkResourcePtr &camera, qint64 timestamp, bool searchForward, QnTimePeriod* foundPeriod) const
{
    Q_ASSERT(foundPeriod);
    QMutexLocker lock(&m_mutex);
    foundPeriod->clear();
    const auto& itr = m_historyDetail.find(camera->getId());
    if (itr == m_historyDetail.end())
        return QnMediaServerResourcePtr(); // no history data, there is'nt  next server
    const auto detailData = filterOnlineServers(itr.value());
    auto detailItr = getMediaServerOnTimeInternal(detailData, timestamp);
    if (detailItr == detailData.end())
        return  QnMediaServerResourcePtr();
    qint64 t1 = detailItr->timestampMs;

    if (searchForward) {
        if (++detailItr == detailData.end())
            return  QnMediaServerResourcePtr();
        foundPeriod->startTimeMs = detailItr->timestampMs;
        if (++detailItr == detailData.end())
            return  QnMediaServerResourcePtr();
        foundPeriod->durationMs = detailItr->timestampMs - foundPeriod->startTimeMs;
    }
    else {
        if (detailItr == detailData.cbegin())
            return QnMediaServerResourcePtr();
        else {
            --detailItr;
            foundPeriod->startTimeMs = detailItr->timestampMs;
            foundPeriod->durationMs = t1 - foundPeriod->startTimeMs;
        }
    }
    return toMediaServer(detailItr->serverGuid);
}

void QnCameraHistoryPool::setCamerasWithArchiveList(const ec2::ApiCameraHistoryDataList& cameraHistoryList)
{
    QMutexLocker lock(&m_mutex);
    m_archivedCameras.clear();
    for(const ec2::ApiCameraHistoryData& item: cameraHistoryList)
        setCamerasWithArchiveNoLock(item.serverGuid, item.archivedCameras);
}

void QnCameraHistoryPool::setCamerasWithArchive(const QnUuid& serverGuid, const std::vector<QnUuid>& cameras)
{
    QMutexLocker lock(&m_mutex);
    setCamerasWithArchiveNoLock(serverGuid, cameras);
}

void QnCameraHistoryPool::setCamerasWithArchiveNoLock(const QnUuid& serverGuid, const std::vector<QnUuid>& cameras)
{
    m_archivedCameras.insert(serverGuid, cameras);
}

std::vector<QnUuid> QnCameraHistoryPool::getCamerasWithArchive(const QnUuid& serverGuid) const
{
    QMutexLocker lock(&m_mutex);
    std::vector<QnUuid> result;
    for(const auto& value: m_archivedCameras.value(serverGuid))
        result.push_back(value);
    return result;
}
