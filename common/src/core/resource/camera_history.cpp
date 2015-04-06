#include "camera_history.h"

#include <api/helpers/chunks_request_data.h>

#include <nx_ec/data/api_camera_history_data.h>

#include <core/resource_management/resource_pool.h>
#include <core/resource/network_resource.h>
#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>

#include <utils/common/util.h>
#include <utils/common/delayed.h>

// ------------------- CameraHistory Pool ------------------------

QnCameraHistoryPool::QnCameraHistoryPool(QObject *parent):
    QObject(parent),
    m_mutex(QMutex::Recursive)
{}

QnCameraHistoryPool::~QnCameraHistoryPool() {}

QnMediaServerResourcePtr QnCameraHistoryPool::getCurrentServer(const QnVirtualCameraResourcePtr &camera) const {
    return camera->getParentResource().dynamicCast<QnMediaServerResource>();
}

bool QnCameraHistoryPool::isCameraDataLoaded(const QnVirtualCameraResourcePtr &camera) const {
    auto itr = m_historyDetail.find(camera->getId());
    return (itr != m_historyDetail.cend());
}

bool QnCameraHistoryPool::prepareCamera(const QnVirtualCameraResourcePtr &camera, callbackFunction handler) {
    if (isCameraDataLoaded(camera)) {
        executeDelayed([handler] {
            handler(true);
        });
        return true;
    }

    //TODO: #GDM #history_refactor Current server we are connected to - it is online.
    QnMediaServerResourcePtr server = camera->getParentResource().dynamicCast<QnMediaServerResource>();
    if (!server)
        return false;

    auto connection = server->apiConnection();
    if (!connection)
        return false;

    //ec2/cameraHistory

    QnChunksRequestData request;
    request.resList << camera;

    int handle = connection->cameraHistory(request, this, SLOT(at_cameraPrepared(int, const ec2::ApiCameraHistoryDataList &, int)));
    if (handle < 0)
        return false;

    m_loadingCameras[handle] = handler;

    return true;
}

void QnCameraHistoryPool::at_cameraPrepared(int status, const ec2::ApiCameraHistoryDataList &periods, int handle) {
    bool success = (status == 0);

    for (const auto &detail: periods)
        m_historyDetail[detail.cameraId] = detail.items;

    if (!m_loadingCameras.contains(handle))
        return;
    auto handler = m_loadingCameras.take(handle);
    handler(success);
}


QnMediaServerResourceList QnCameraHistoryPool::getCameraFootageData(const QnVirtualCameraResourcePtr &camera) const {
    QnMediaServerResourceList result;
    QnUuid cameraId = camera->getId();
    for(auto itr = m_archivedCamerasByServer.cbegin(); itr != m_archivedCamerasByServer.cend(); ++itr) {
        const auto &data = itr.value();
        if (std::find(data.cbegin(), data.cend(), cameraId) != data.cend()) {
            QnMediaServerResourcePtr mServer = toMediaServer(itr.key());
            if (mServer)
                result << mServer;
        }
    }
    return result;
}

QnMediaServerResourceList QnCameraHistoryPool::getCameraFootageData(const QnVirtualCameraResourcePtr &camera, const QnTimePeriod& timePeriod) const {
    if (!isCameraDataLoaded(camera))
        return getCameraFootageData(camera);

    //TODO: #history_refactor double find
    auto itr = m_historyDetail.find(camera->getId());
    const auto& moveData = itr.value();
    if (moveData.empty())
        return QnMediaServerResourceList();
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

ec2::ApiCameraHistoryItemDataList::const_iterator QnCameraHistoryPool::getMediaServerOnTimeInternal(const ec2::ApiCameraHistoryItemDataList& data, qint64 timestamp) const
{
    //TODO: #history_refactor test me!!!
    // todo: test me!!!
    return std::lower_bound(data.begin(), data.end(), timestamp, [](const ec2::ApiCameraHistoryItemData& data, qint64 timestamp) { 
        return timestamp < data.timestampMs; // inverse compare
    });
}

ec2::ApiCameraHistoryItemDataList QnCameraHistoryPool::filterOnlineServers(const ec2::ApiCameraHistoryItemDataList& dataList) const
{
    ec2::ApiCameraHistoryItemDataList result;
    std::copy_if(dataList.cbegin(), dataList.cend(), std::back_inserter(result), [](const ec2::ApiCameraHistoryItemData &data)
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

QnMediaServerResourcePtr QnCameraHistoryPool::getMediaServerOnTime(const QnVirtualCameraResourcePtr &camera, qint64 timestamp, QnTimePeriod* foundPeriod) const
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

QnMediaServerResourcePtr QnCameraHistoryPool::getNextMediaServerAndPeriodOnTime(const QnVirtualCameraResourcePtr &camera, qint64 timestamp, bool searchForward, QnTimePeriod* foundPeriod) const
{
    Q_ASSERT_X(foundPeriod, Q_FUNC_INFO, "target period MUST be present");
    if (!foundPeriod)
        return getMediaServerOnTime(camera, timestamp);

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

void QnCameraHistoryPool::resetServerFootageData(const ec2::ApiServerFootageDataList& cameraHistoryList)
{
    QMutexLocker lock(&m_mutex);
    m_archivedCamerasByServer.clear();
    for(const ec2::ApiServerFootageData& item: cameraHistoryList)
        setServerFootageDataNoLock(item.serverGuid, item.archivedCameras);
}

void QnCameraHistoryPool::setServerFootageData(const QnUuid& serverGuid, const std::vector<QnUuid>& cameras)
{
    QMutexLocker lock(&m_mutex);
    setServerFootageDataNoLock(serverGuid, cameras);
}

void QnCameraHistoryPool::setServerFootageData(const ec2::ApiServerFootageData &serverFootageData)
{
    setServerFootageData(serverFootageData.serverGuid, serverFootageData.archivedCameras);
}

void QnCameraHistoryPool::setServerFootageDataNoLock(const QnUuid& serverGuid, const std::vector<QnUuid>& cameras)
{
    m_archivedCamerasByServer.insert(serverGuid, cameras);
}

std::vector<QnUuid> QnCameraHistoryPool::getServerFootageData(const QnUuid& serverGuid) const {
    QMutexLocker lock(&m_mutex);
    std::vector<QnUuid> result;
    for(const auto& value: m_archivedCamerasByServer.value(serverGuid))
        result.push_back(value);
    return result;
}
