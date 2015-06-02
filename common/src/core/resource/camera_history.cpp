#include "camera_history.h"

#include <api/helpers/chunks_request_data.h>

#include <common/common_module.h>

#include <nx_ec/data/api_camera_history_data.h>

#include <core/resource_management/resource_pool.h>
#include <core/resource/network_resource.h>
#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>

#include <utils/common/util.h>
#include <utils/common/delayed.h>

namespace {

    /** Current server for the given camera. */
    QnMediaServerResourcePtr currentServerForCamera(const QnVirtualCameraResourcePtr &camera) {
        return camera->getParentResource().dynamicCast<QnMediaServerResource>();
    }

    ec2::ApiCameraHistoryItemDataList::const_iterator getMediaServerOnTimeInternal(const ec2::ApiCameraHistoryItemDataList& data, qint64 timestamp) {
        /* Find first data with timestamp not less than given. */
        auto iter = std::lower_bound(data.cbegin(), data.cend(), timestamp, [](const ec2::ApiCameraHistoryItemData& data, qint64 timestamp) { 
            return data.timestampMs < timestamp;
        });

        /* Check exact match. */
        if (iter != data.cend() && iter->timestampMs == timestamp)
            return iter;

        /* Check if the first data is already greater than required. */
        if (iter == data.cbegin())
            return data.cend();

        /* Otherwise get previous server. */
        if (data.cbegin() != iter)
            --iter;

        return iter;
    }
}

// ------------------- CameraHistory Pool ------------------------

QnCameraHistoryPool::QnCameraHistoryPool(QObject *parent):
    QObject(parent),
    m_mutex(QMutex::Recursive)
{
    connect(qnResPool, &QnResourcePool::statusChanged, this, [this](const QnResourcePtr &resource) {
        /* Fast check. */
        if (!resource->hasFlags(Qn::remote_server))
            return;

        auto cameras = getServerFootageData(resource->getId());

        /* Do not invalidate history if server goes offline. */
        if (resource->getStatus() == Qn::Online)
            for (const auto &cameraId: cameras)
                invalidateCameraHistory(cameraId);

        for (const auto &cameraId: cameras)
            if (QnVirtualCameraResourcePtr camera = toCamera(cameraId))
                emit cameraFootageChanged(camera);
    });
}

QnCameraHistoryPool::~QnCameraHistoryPool() {}

bool QnCameraHistoryPool::isCameraHistoryValid(const QnVirtualCameraResourcePtr &camera) const {
    QnMutexLocker lock( &m_mutex );
    return m_historyValidCameras.contains(camera->getId());
}

void QnCameraHistoryPool::invalidateCameraHistory(const QnUuid &cameraId) {
    bool notify = false;

    {
        QnMutexLocker lock( &m_mutex );
        notify = m_historyValidCameras.contains(cameraId);
        m_historyValidCameras.remove(cameraId);
    }

    if (notify)
        if (QnVirtualCameraResourcePtr camera = toCamera(cameraId))
            emit cameraHistoryInvalidated(camera);
}

bool QnCameraHistoryPool::updateCameraHistoryAsync(const QnVirtualCameraResourcePtr &camera, callbackFunction callback) {
    if (isCameraHistoryValid(camera)) {
        executeDelayed([callback] {
            callback(true);
        });
        return true;
    }

    QnMediaServerResourcePtr server = qnCommon->currentServer();
    if (!server)
        return false;

    auto connection = server->apiConnection();
    if (!connection)
        return false;

    QnChunksRequestData request;
    request.resList << camera;

    int handle = connection->cameraHistory(request, this, SLOT(at_cameraPrepared(int, const ec2::ApiCameraHistoryDataList &, int)));
    if (handle < 0)
        return false;

    {
        QMutexLocker lock(&m_mutex);
        m_runningRequests[handle] = callback;
    }

    return true;
}

void QnCameraHistoryPool::at_cameraPrepared(int status, const ec2::ApiCameraHistoryDataList &periods, int handle) {
    QMutexLocker lock(&m_mutex);

    QSet<QnUuid> loadedCamerasIds;
    bool success = (status == 0);
    if (success) {
        for (const auto &detail: periods) {

            /* 
            * Make sure server has received data from all servers that are online.
            * Note that online list on the client and on the server CAN BE DIFFERENT. 
            */
            QSet<QnUuid> receivedServers;

            m_historyDetail[detail.cameraId] = detail.items;
            for (const auto &item: detail.items)
                receivedServers.insert(item.serverGuid);

            QnMediaServerResourceList footageServers = getCameraFootageData(detail.cameraId);
            bool isResultValid = std::all_of(footageServers.cbegin(), footageServers.cend(), 
                [receivedServers](const QnMediaServerResourcePtr &server) {
                    /* Ignore offline servers. */
                    if (server->getStatus() != Qn::Online)
                        return true;
                    return (receivedServers.contains(server->getId()));
            });

            //TODO: #GDM history_refactor remove when debugging will be finished.
            Q_ASSERT_X(isResultValid, Q_FUNC_INFO, "Normally, we shouldn't get here. It is possible, but not in the local network.");
            if (isResultValid)
                m_historyValidCameras.insert(detail.cameraId);

            loadedCamerasIds.insert(detail.cameraId);
        }  
    }

    if (!m_runningRequests.contains(handle))
        return;
    auto handler = m_runningRequests.take(handle);

    lock.unlock();

    handler(success);
    for (const QnUuid &cameraId: loadedCamerasIds)
        if (QnVirtualCameraResourcePtr camera = toCamera(cameraId))
            emit cameraHistoryChanged(camera);
}

QnMediaServerResourceList QnCameraHistoryPool::getCameraFootageData(const QnUuid &cameraId) const {
    QMutexLocker lock(&m_mutex);

    QnMediaServerResourceList result;
    for(auto itr = m_archivedCamerasByServer.cbegin(); itr != m_archivedCamerasByServer.cend(); ++itr) {
        const auto &data = itr.value();
        if (std::find(data.cbegin(), data.cend(), cameraId) != data.cend()) {
            QnMediaServerResourcePtr server = toMediaServer(itr.key());
            if (server)
                result << server;
        }
    }

    return result;
}

QnMediaServerResourceList QnCameraHistoryPool::getCameraFootageData(const QnVirtualCameraResourcePtr &camera) const {
    return getCameraFootageData(camera->getId());
}

QnMediaServerResourceList QnCameraHistoryPool::getCameraFootageData(const QnVirtualCameraResourcePtr &camera, const QnTimePeriod& timePeriod) const {
    if (!isCameraHistoryValid(camera))
        return getCameraFootageData(camera);

    QnMutexLocker lock( &m_mutex );
    auto itr = m_historyDetail.find(camera->getId());
    const auto& moveData = itr.value();
    if (moveData.empty())
        return QnMediaServerResourceList();

    auto itr2 = getMediaServerOnTimeInternal(moveData, timePeriod.startTimeMs);   
    /* itr2 can be empty in case when the timePeriod starts after the last camera movement. */

    QSet<QnMediaServerResourcePtr> result;
    while (itr2 != moveData.end() && itr2->timestampMs < timePeriod.endTimeMs()) {
        QnMediaServerResourcePtr server = toMediaServer(itr2->serverGuid);
        if (server)
            result << server;
        ++itr2;
    }

    return result.toList();
}

ec2::ApiCameraHistoryItemDataList QnCameraHistoryPool::filterOnlineServers(const ec2::ApiCameraHistoryItemDataList& dataList) const {
    ec2::ApiCameraHistoryItemDataList result;
    std::copy_if(dataList.cbegin(), dataList.cend(), std::back_inserter(result), [this](const ec2::ApiCameraHistoryItemData &data)
    {
        QnMediaServerResourcePtr res = toMediaServer(data.serverGuid);
        return res && res->getStatus() == Qn::Online;
    });
    return result;
}

QnVirtualCameraResourcePtr QnCameraHistoryPool::toCamera(const QnUuid& guid) const {
    return qnResPool->getResourceById(guid).dynamicCast<QnVirtualCameraResource>();
}

QnMediaServerResourcePtr QnCameraHistoryPool::toMediaServer(const QnUuid& guid) const {
    return qnResPool->getResourceById(guid).dynamicCast<QnMediaServerResource>();
}

QnMediaServerResourcePtr QnCameraHistoryPool::getMediaServerOnTime(const QnVirtualCameraResourcePtr &camera, qint64 timestamp, QnTimePeriod* foundPeriod) const {
    if (foundPeriod)
        foundPeriod->clear();

    QMutexLocker lock(&m_mutex);
    const auto& itr = m_historyDetail.find(camera->getId());
    if (itr == m_historyDetail.end())
        return currentServerForCamera(camera); // no history data. use current server constantly

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

QnMediaServerResourcePtr QnCameraHistoryPool::getNextMediaServerAndPeriodOnTime(const QnVirtualCameraResourcePtr &camera, qint64 timestamp, bool searchForward, QnTimePeriod* foundPeriod) const {
    Q_ASSERT_X(foundPeriod, Q_FUNC_INFO, "target period MUST be present");
    if (!foundPeriod)
        return getMediaServerOnTime(camera, timestamp);

    foundPeriod->clear();
    const auto& itr = m_historyDetail.find(camera->getId());
    if (itr == m_historyDetail.end())
        return QnMediaServerResourcePtr(); // no history data, there isn't  next server
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
    QSet<QnUuid> localHistoryValidCameras;
    QSet<QnUuid> allCameras;
    {
        QnMutexLocker lock( &m_mutex );
        m_archivedCamerasByServer.clear();
        m_historyDetail.clear();
        localHistoryValidCameras = m_historyValidCameras;
        for(const ec2::ApiServerFootageData& item: cameraHistoryList) {
            m_archivedCamerasByServer.insert(item.serverGuid, item.archivedCameras);
            for (const auto &cameraId: item.archivedCameras)
                allCameras.insert(cameraId);
        }
    }

    for (const QnUuid &cameraId: localHistoryValidCameras)
        invalidateCameraHistory(cameraId);

    for (const QnUuid &cameraId: allCameras) 
        if (QnVirtualCameraResourcePtr camera = toCamera(cameraId))
            emit cameraFootageChanged(camera);
}

void QnCameraHistoryPool::setServerFootageData(const QnUuid& serverGuid, const std::vector<QnUuid>& cameras) {
    {
        QMutexLocker lock(&m_mutex);
        m_archivedCamerasByServer.insert(serverGuid, cameras);
    }
    for (const auto &cameraId : cameras) {
        invalidateCameraHistory(cameraId);
        if (QnVirtualCameraResourcePtr camera = toCamera(cameraId))
            emit cameraFootageChanged(camera);
    }
}

void QnCameraHistoryPool::setServerFootageData(const ec2::ApiServerFootageData &serverFootageData) {
    setServerFootageData(serverFootageData.serverGuid, serverFootageData.archivedCameras);
}

std::vector<QnUuid> QnCameraHistoryPool::getServerFootageData(const QnUuid& serverGuid) const {
    QnMutexLocker lock( &m_mutex );
    std::vector<QnUuid> result;
    for(const auto& value: m_archivedCamerasByServer.value(serverGuid))
        result.push_back(value);
    return result;
}
