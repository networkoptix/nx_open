#include "camera_history.h"

#include <api/helpers/chunks_request_data.h>

#include <common/common_module.h>

#include <nx_ec/data/api_camera_history_data.h>

#include <core/resource_management/resource_pool.h>
#include <core/resource/network_resource.h>
#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>
#include "api/server_rest_connection.h"

#include <utils/common/util.h>
#include <utils/common/delayed.h>
#include "utils/common/synctime.h"
#include <api/common_message_processor.h>
#include <business/actions/abstract_business_action.h>
#include <business/business_action_parameters.h>
#include <health/system_health.h>


namespace {
    static const int HistoryCheckDelay = 1000 * 15;

    ec2::ApiCameraHistoryItemDataList::const_iterator getMediaServerOnTimeInternal(const ec2::ApiCameraHistoryItemDataList& data, qint64 timestamp) {
        /* Find first data with timestamp not less than given. */
        auto iter = std::lower_bound(data.cbegin(), data.cend(), timestamp, [](const ec2::ApiCameraHistoryItemData& data, qint64 timestamp) {
            return data.timestampMs < timestamp;
        });

        /* Check exact match. */
        if (iter != data.cend() && iter->timestampMs == timestamp)
            return iter;

        /* get previous server. */
        if (data.cbegin() != iter)
            --iter;

        return iter;
    }
}

// ------------------- CameraHistory Pool ------------------------


void QnCameraHistoryPool::checkCameraHistoryDelayed(QnVirtualCameraResourcePtr cam)
{
    /*
    * When camera goes to a recording state it's expected that current history server is the same as parentID.
    * We should check it because in case of camera is moved several times between 2 servers,
    * there is no cameraHistoryChanged signal expected.
    * Check it after some delay to avoid call 'invalidateCameraHistory' several times in a row.
    * It could be because client video camera sends extra 'statusChanged' signal when camera is moved.
    * Also, online->recording may occurs on the server side before history information updated.
    * Excact delay isn't important, at worse scenario we just do extra work. So, do it delayed.
    */

    if (cam->getStatus() != Qn::Recording) {
        m_camerasToCheck.remove(cam->getId());
        return;
    }

    QnUuid id = cam->getId();
    if (m_camerasToCheck.contains(id))
        return;
    m_camerasToCheck << cam->getId();
    executeDelayed([this, id]()
    {
        if (!m_camerasToCheck.contains(id))
            return;
        m_camerasToCheck.remove(id);

        QnVirtualCameraResourcePtr cam = qnResPool->getResourceById<QnVirtualCameraResource>(id);
        if (!cam)
            return;

        auto server = getMediaServerOnTime(cam, qnSyncTime->currentMSecsSinceEpoch());
        if (cam && server && server->getId() != cam->getParentId())
            invalidateCameraHistory(id);
    }, HistoryCheckDelay);
}

QnCameraHistoryPool::QnCameraHistoryPool(QObject *parent):
    QObject(parent),
    m_mutex(QnMutex::Recursive)
{
    connect(qnResPool, &QnResourcePool::statusChanged, this, [this](const QnResourcePtr &resource) {
        QnVirtualCameraResourcePtr cam = qnResPool->getResourceById<QnVirtualCameraResource>(resource->getId());
        if (cam)
            checkCameraHistoryDelayed(cam);

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
    QnCommonMessageProcessor *messageProcessor = QnCommonMessageProcessor::instance();
    connect(messageProcessor,   &QnCommonMessageProcessor::businessActionReceived, this,
            [this] (const QnAbstractBusinessActionPtr &businessAction)
    {
        QnBusiness::EventType eventType = businessAction->getRuntimeParams().eventType;
        if (eventType >= QnBusiness::SystemHealthEvent && eventType <= QnBusiness::MaxSystemHealthEvent) {
            QnSystemHealth::MessageType healthMessage = QnSystemHealth::MessageType(eventType - QnBusiness::SystemHealthEvent);
            if (healthMessage == QnSystemHealth::ArchiveRebuildFinished || healthMessage == QnSystemHealth::ArchiveFastScanFinished)
            {
                auto cameras = getServerFootageData(businessAction->getRuntimeParams().eventResourceId);
                for (const auto &cameraId: cameras)
                    invalidateCameraHistory(cameraId);

                for (const auto &cameraId: cameras)
                    if (QnVirtualCameraResourcePtr camera = toCamera(cameraId))
                        emit cameraFootageChanged(camera);
            }
        }
    });
}

QnCameraHistoryPool::~QnCameraHistoryPool() {}

bool QnCameraHistoryPool::isCameraHistoryValid(const QnVirtualCameraResourcePtr &camera) const {
    QnMutexLocker lock( &m_mutex );
    return m_historyValidCameras.contains(camera->getId());
}

void QnCameraHistoryPool::invalidateCameraHistory(const QnUuid &cameraId) {
    bool notify = false;

    QnMediaServerResourcePtr server = qnCommon->currentServer();
    if (!server)
        return; // somethink wrong

    {
        QnMutexLocker lock( &m_mutex );
        notify = m_historyValidCameras.contains(cameraId);
        m_historyValidCameras.remove(cameraId);
        if (m_asyncRunningRequests.contains(cameraId)) {
            server->restConnection()->cancelRequest(m_asyncRunningRequests[cameraId]);
            m_asyncRunningRequests.remove(cameraId);
            notify = true;
        }
    }

    if (notify)
        if (QnVirtualCameraResourcePtr camera = toCamera(cameraId))
            emit cameraHistoryInvalidated(camera);
}


QnCameraHistoryPool::StartResult QnCameraHistoryPool::updateCameraHistoryAsync(const QnVirtualCameraResourcePtr &camera, callbackFunction callback)
{
    Q_ASSERT_X(!camera.isNull(), Q_FUNC_INFO, "Camera resource is null!");
    if (camera.isNull())
        return StartResult::failed;

    if (isCameraHistoryValid(camera))
        return StartResult::ommited;

    QnMediaServerResourcePtr server = qnCommon->currentServer();
    if (!server)
        return StartResult::failed;

    QnChunksRequestData request;
    request.format = Qn::UbjsonFormat;
    request.resList << camera;

    QnMutexLocker lock(&m_mutex);
    using namespace std::placeholders;
    auto handle = server->restConnection()->cameraHistoryAsync(request, [this, callback] (bool success, rest::Handle id, const ec2::ApiCameraHistoryDataList &periods)
    {
        at_cameraPrepared(success, id, periods, callback);
    });
    bool started = handle > 0;
    if (started)
        m_asyncRunningRequests.insert(camera->getId(), handle);

    lock.unlock();

    return started ? StartResult::started : StartResult::failed;
}

void QnCameraHistoryPool::at_cameraPrepared(bool success, const rest::Handle& requestId, const ec2::ApiCameraHistoryDataList &periods, callbackFunction callback)
{
    Q_UNUSED(requestId);
    QnMutexLocker lock(&m_mutex);
    for (auto itr = m_asyncRunningRequests.begin(); itr != m_asyncRunningRequests.end(); ++itr) {
        if (itr.value() == requestId) {
            m_asyncRunningRequests.erase(itr);
            break;
        }
    }

    QSet<QnUuid> loadedCamerasIds;
    if (success) {
        for (const auto &detail: periods)
        {
            m_historyDetail[detail.cameraId] = detail.items;
            m_historyValidCameras.insert(detail.cameraId);
            loadedCamerasIds.insert(detail.cameraId);
        }
    }


    lock.unlock();
    if (callback)
        executeDelayed([callback, success] { callback(success); }, kDefaultDelay, this->thread());

    for (const QnUuid &cameraId: loadedCamerasIds)
        if (QnVirtualCameraResourcePtr camera = toCamera(cameraId))
            emit cameraHistoryChanged(camera);
}

QnMediaServerResourceList QnCameraHistoryPool::getCameraFootageData(const QnUuid &cameraId, bool filterOnlineServers) const {
    QnMutexLocker lock(&m_mutex);

    QnMediaServerResourceList result;
    for(auto itr = m_archivedCamerasByServer.cbegin(); itr != m_archivedCamerasByServer.cend(); ++itr) {
        const auto &data = itr.value();
        if (std::find(data.cbegin(), data.cend(), cameraId) != data.cend()) {
            QnMediaServerResourcePtr server = toMediaServer(itr.key());
            if (server && (!filterOnlineServers || server->getStatus() == Qn::Online))
                result << server;
        }
    }

    return result;
}

QnMediaServerResourceList QnCameraHistoryPool::dtsCamFootageData(const QnVirtualCameraResourcePtr &camera
    , bool filterOnlineServers) const
{
    Q_ASSERT_X(!camera.isNull(), Q_FUNC_INFO, "Camera resource is null!");
    if (camera.isNull())
        return QnMediaServerResourceList();

    QnMediaServerResourceList result;
    auto server = toMediaServer(camera->getParentId());
    if (server && (!filterOnlineServers || (server->getStatus() == Qn::Online)))
        result << server;
    return result;
}

QnMediaServerResourceList QnCameraHistoryPool::getCameraFootageData(const QnVirtualCameraResourcePtr &camera, bool filterOnlineServers) const
{
    Q_ASSERT_X(!camera.isNull(), Q_FUNC_INFO, "Camera resource is null!");
    if (camera.isNull())
        return QnMediaServerResourceList();

    if (camera->isDtsBased())
        return dtsCamFootageData(camera, filterOnlineServers);
    else
        return getCameraFootageData(camera->getId(), filterOnlineServers);
}

QnMediaServerResourceList QnCameraHistoryPool::getCameraFootageData(const QnVirtualCameraResourcePtr &camera, const QnTimePeriod& timePeriod) const
{
    Q_ASSERT_X(!camera.isNull(), Q_FUNC_INFO, "Camera resource is null!");
    if (camera.isNull())
        return QnMediaServerResourceList();

    if (camera->isDtsBased())
        return dtsCamFootageData(camera);

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

QnVirtualCameraResourcePtr QnCameraHistoryPool::toCamera(const QnUuid& guid) const
{
    return qnResPool->getResourceById(guid).dynamicCast<QnVirtualCameraResource>();
}

QnMediaServerResourcePtr QnCameraHistoryPool::toMediaServer(const QnUuid& guid) const
{
    return qnResPool->getResourceById(guid).dynamicCast<QnMediaServerResource>();
}

QnMediaServerResourcePtr QnCameraHistoryPool::getMediaServerOnTime(const QnVirtualCameraResourcePtr &camera, qint64 timestamp, QnTimePeriod* foundPeriod) const
{
    Q_ASSERT_X(!camera.isNull(), Q_FUNC_INFO, "Camera resource is null!");
    if (camera.isNull())
        return QnMediaServerResourcePtr();

    if (foundPeriod)
        foundPeriod->clear();

    QnMutexLocker lock(&m_mutex);
    const auto& itr = m_historyDetail.find(camera->getId());
    if (itr == m_historyDetail.end() || itr.value().empty())
        return camera->getParentServer(); // no history data. use current server constantly

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

QnMediaServerResourcePtr QnCameraHistoryPool::getMediaServerOnTimeSync(const QnVirtualCameraResourcePtr &camera, qint64 timestampMs, QnTimePeriod* foundPeriod)
{
    Q_ASSERT_X(!camera.isNull(), Q_FUNC_INFO, "Camera resource is null!");
    if (camera.isNull())
        return QnMediaServerResourcePtr();

    updateCameraHistorySync(camera);
    return getMediaServerOnTime(camera, timestampMs, foundPeriod);
}

QnMediaServerResourcePtr QnCameraHistoryPool::getNextMediaServerAndPeriodOnTime(const QnVirtualCameraResourcePtr &camera, qint64 timestamp, bool searchForward, QnTimePeriod* foundPeriod) const
{
    Q_ASSERT_X(!camera.isNull(), Q_FUNC_INFO, "Camera resource is null!");
    if (camera.isNull())
        return QnMediaServerResourcePtr();

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
        auto nextServerItr = detailItr;
        if (++nextServerItr == detailData.end())
            foundPeriod->durationMs = QnTimePeriod::UnlimitedPeriod;
        else
            foundPeriod->durationMs = nextServerItr->timestampMs - foundPeriod->startTimeMs;
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
        QnMutexLocker lock(&m_mutex);
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
    return m_archivedCamerasByServer.value(serverGuid);
}

QnVirtualCameraResourceList QnCameraHistoryPool::getServerFootageCameras( const QnMediaServerResourcePtr &server ) const {
    QnVirtualCameraResourceList result;
    if (!server)
        return result;

    for (const QnUuid &cameraId: getServerFootageData(server->getId())) {
        if (QnVirtualCameraResourcePtr camera = toCamera(cameraId))
            result << camera;
    }

    return result;
}


bool QnCameraHistoryPool::updateCameraHistorySync(const QnVirtualCameraResourcePtr &camera)
{
    if (!camera)
        return true;

    QnMutexLocker lock(&m_syncLoadMutex);
    while (m_syncRunningRequests.contains(camera->getId()))
        m_syncLoadWaitCond.wait(&m_syncLoadMutex);

    if (isCameraHistoryValid(camera))
        return true;

    StartResult result = updateCameraHistoryAsync(camera, [this, camera] (bool success)
    {
        QnMutexLocker lock(&m_syncLoadMutex);
        m_syncRunningRequests.remove(camera->getId());
        m_syncLoadWaitCond.wakeAll();
    });

    if (result == StartResult::started) {
        m_syncRunningRequests.insert(camera->getId());
        m_syncLoadWaitCond.wait(&m_syncLoadMutex);
    }
    return result != StartResult::failed;
}
