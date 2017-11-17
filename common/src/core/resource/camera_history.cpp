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
#include <nx/vms/event/actions/abstract_action.h>
#include <nx/vms/event/action_parameters.h>
#include <health/system_health.h>
#include <nx/utils/log/log.h>
#include <common/common_module.h>

using namespace nx;

namespace {
    static const int kDefaultHistoryCheckDelay = 1000 * 15;

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


void QnCameraHistoryPool::setHistoryCheckDelay(int value)
{
    m_historyCheckDelay = value;
}

void QnCameraHistoryPool::checkCameraHistoryDelayed(QnSecurityCamResourcePtr cam)
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

    const auto timerCallback =
        [this, id]()
        {
            if (!m_camerasToCheck.contains(id))
                return;

            m_camerasToCheck.remove(id);

            QnSecurityCamResourcePtr cam = commonModule()->resourcePool()->getResourceById<QnSecurityCamResource>(id);
            if (!cam)
                return;

            auto server = getMediaServerOnTime(cam, qnSyncTime->currentMSecsSinceEpoch());
            if (cam && server && server->getId() != cam->getParentId())
                invalidateCameraHistory(id);
        };

    executeDelayedParented(timerCallback, m_historyCheckDelay, this);
}

QnCameraHistoryPool::QnCameraHistoryPool(QObject *parent):
    QObject(parent),
    QnCommonModuleAware(parent),
    m_historyCheckDelay(kDefaultHistoryCheckDelay),
    m_mutex(QnMutex::Recursive)
{
    connect(commonModule()->resourcePool(), &QnResourcePool::statusChanged, this, [this](const QnResourcePtr &resource)
    {

        NX_LOG(lit("%1 statusChanged signal received for resource %2, %3, %4")
                .arg(QString::fromLatin1(Q_FUNC_INFO))
                .arg(resource->getId().toString())
                .arg(resource->getName())
                .arg(resource->getUrl()), cl_logDEBUG2);

        QnSecurityCamResourcePtr cam = resource.dynamicCast<QnSecurityCamResource>();
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
            if (QnSecurityCamResourcePtr camera = toCamera(cameraId))
                emit cameraFootageChanged(camera);
    });
}

void QnCameraHistoryPool::setMessageProcessor(const QnCommonMessageProcessor* messageProcessor)
{
    if (m_messageProcessor)
        disconnect(m_messageProcessor, nullptr, this, nullptr);

    if (messageProcessor)
    {
        connect(messageProcessor, &QnCommonMessageProcessor::businessActionReceived, this,
            [this](const vms::event::AbstractActionPtr& businessAction)
            {
                vms::event::EventType eventType = businessAction->getRuntimeParams().eventType;
                if (eventType >= vms::event::systemHealthEvent && eventType <= vms::event::maxSystemHealthEvent)
                {
                    QnSystemHealth::MessageType healthMessage = QnSystemHealth::MessageType(eventType - vms::event::systemHealthEvent);
                    if (healthMessage == QnSystemHealth::ArchiveRebuildFinished
                        || healthMessage == QnSystemHealth::ArchiveFastScanFinished
                        || healthMessage == QnSystemHealth::RemoteArchiveSyncFinished
                        || healthMessage == QnSystemHealth::RemoteArchiveSyncProgress)
                    {
                        auto eventParams = businessAction->getRuntimeParams();
                        std::vector<QnUuid> cameras;

                        if (healthMessage == QnSystemHealth::RemoteArchiveSyncFinished
                            || healthMessage == QnSystemHealth::RemoteArchiveSyncProgress)
                        {
                            if (eventParams.metadata.cameraRefs.empty())
                                return;

                            cameras = eventParams.metadata.cameraRefs;
                        }
                        else
                        {
                            cameras = getServerFootageData(eventParams.eventResourceId);
                        }

                        for (const auto &cameraId: cameras)
                            invalidateCameraHistory(cameraId);

                        for (const auto &cameraId : cameras)
                            if (QnSecurityCamResourcePtr camera = toCamera(cameraId))
                                emit cameraFootageChanged(camera);
                    }
                }
            });
    }
    m_messageProcessor = messageProcessor;
}

QnCameraHistoryPool::~QnCameraHistoryPool() {}

bool QnCameraHistoryPool::isCameraHistoryValid(const QnSecurityCamResourcePtr &camera) const {
    QnMutexLocker lock( &m_mutex );
    return m_historyValidCameras.contains(camera->getId());
}

void QnCameraHistoryPool::invalidateCameraHistory(const QnUuid &cameraId) {
    bool notify = false;

    QnMediaServerResourcePtr server = commonModule()->currentServer();
    if (!server)
        return; // somethink wrong

    rest::Handle requestToTerminate = 0;
    {
        QnMutexLocker lock2(&m_syncLoadMutex);
        QnMutexLocker lock( &m_mutex );
        notify = m_historyValidCameras.contains(cameraId);
        m_historyValidCameras.remove(cameraId);
        if (m_asyncRunningRequests.contains(cameraId))
        {
            requestToTerminate = m_asyncRunningRequests[cameraId];
            m_asyncRunningRequests.remove(cameraId);
            notify = true;

            // terminate sync request
            m_syncRunningRequests.remove(cameraId);
            m_syncLoadWaitCond.wakeAll();
        }
    }
    if (requestToTerminate > 0)
        server->restConnection()->cancelRequest(requestToTerminate);


    if (notify)
        if (QnSecurityCamResourcePtr camera = toCamera(cameraId))
            emit cameraHistoryInvalidated(camera);
}


QnCameraHistoryPool::StartResult QnCameraHistoryPool::updateCameraHistoryAsync(const QnSecurityCamResourcePtr &camera, callbackFunction callback)
{
    NX_ASSERT(!camera.isNull(), Q_FUNC_INFO, "Camera resource is null!");
    if (camera.isNull())
        return StartResult::failed;

    if (isCameraHistoryValid(camera))
        return StartResult::ommited;

    QnMediaServerResourcePtr server = commonModule()->currentServer();
    if (!server)
        return StartResult::failed;

    // if camera belongs to a single server not need to do API request to build detailed history information. generate it instead.
    QnMediaServerResourceList serverList = getCameraFootageData(camera->getId(), true);
    if (serverList.size() <= 1)
    {
        ec2::ApiCameraHistoryItemDataList items;
        if (serverList.size() == 1)
            items.push_back(ec2::ApiCameraHistoryItemData(serverList.front()->getId(), 0));

        if (testAndSetHistoryDetails(camera->getId(), items))
            return StartResult::ommited;
    }

    QnChunksRequestData request;
    request.format = Qn::UbjsonFormat;
    request.resList << camera;

    {
        QnMutexLocker lock(&m_mutex);
        QPointer<QnCameraHistoryPool> guard(this);

        auto handle = server->restConnection()->cameraHistoryAsync(request,
            [this, callback, guard = QPointer<QnCameraHistoryPool>(this), thread = this->thread()]
            (bool success, rest::Handle id, ec2::ApiCameraHistoryDataList periods)
            {
                if (!guard)
                    return;

                const auto timerCallback =
                    [this, guard, success, id, periods, callback]()
                    {
                        if (guard)
                            at_cameraPrepared(success, id, periods, callback);
                    };

                executeDelayed(timerCallback, kDefaultDelay, thread);
            });

        bool started = handle > 0;
        if (started)
            m_asyncRunningRequests.insert(camera->getId(), handle);

        return started ? StartResult::started : StartResult::failed;
    }
}

void QnCameraHistoryPool::at_cameraPrepared(bool success, const rest::Handle& requestId, const ec2::ApiCameraHistoryDataList &periods, callbackFunction callback)
{
    Q_UNUSED(requestId);
    QnMutexLocker lock(&m_mutex);
    bool requestFound = false;
    for (auto itr = m_asyncRunningRequests.begin(); itr != m_asyncRunningRequests.end(); ++itr) {
        if (itr.value() == requestId) {
            m_asyncRunningRequests.erase(itr);
            requestFound = true;
            break;
        }
    }
    if (!requestFound)
        return; //< request has been canceled

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
        callback(success);

    for (const QnUuid &cameraId: loadedCamerasIds)
        if (QnSecurityCamResourcePtr camera = toCamera(cameraId))
            emit cameraHistoryChanged(camera);
}

QnMediaServerResourceList QnCameraHistoryPool::getCameraFootageData(const QnUuid &cameraId, bool filterOnlineServers) const
{
    QnMutexLocker lock(&m_mutex);
    return getCameraFootageDataUnsafe(cameraId, filterOnlineServers);
}

QnMediaServerResourceList QnCameraHistoryPool::getCameraFootageDataUnsafe(const QnUuid &cameraId, bool filterOnlineServers) const
{
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

QnMediaServerResourceList QnCameraHistoryPool::dtsCamFootageData(const QnSecurityCamResourcePtr &camera
    , bool filterOnlineServers) const
{
    NX_ASSERT(!camera.isNull(), Q_FUNC_INFO, "Camera resource is null!");
    if (camera.isNull())
        return QnMediaServerResourceList();

    QnMediaServerResourceList result;
    auto server = toMediaServer(camera->getParentId());
    if (server && (!filterOnlineServers || (server->getStatus() == Qn::Online)))
        result << server;
    return result;
}

QnMediaServerResourceList QnCameraHistoryPool::getCameraFootageData(const QnSecurityCamResourcePtr &camera, bool filterOnlineServers) const
{
    NX_ASSERT(!camera.isNull(), Q_FUNC_INFO, "Camera resource is null!");
    if (camera.isNull())
        return QnMediaServerResourceList();

    if (camera->isDtsBased())
        return dtsCamFootageData(camera, filterOnlineServers);
    else
        return getCameraFootageData(camera->getId(), filterOnlineServers);
}

QnMediaServerResourceList QnCameraHistoryPool::getCameraFootageData(const QnSecurityCamResourcePtr &camera, const QnTimePeriod& timePeriod) const
{
    NX_ASSERT(!camera.isNull(), Q_FUNC_INFO, "Camera resource is null!");
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

QnSecurityCamResourcePtr QnCameraHistoryPool::toCamera(const QnUuid& guid) const
{
    return commonModule()->resourcePool()->getResourceById(guid).dynamicCast<QnSecurityCamResource>();
}

QnMediaServerResourcePtr QnCameraHistoryPool::toMediaServer(const QnUuid& guid) const
{
    return commonModule()->resourcePool()->getResourceById(guid).dynamicCast<QnMediaServerResource>();
}

QnMediaServerResourcePtr QnCameraHistoryPool::getMediaServerOnTime(const QnSecurityCamResourcePtr &camera, qint64 timestamp, QnTimePeriod* foundPeriod) const
{
    NX_ASSERT(!camera.isNull(), Q_FUNC_INFO, "Camera resource is null!");
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

QnMediaServerResourcePtr QnCameraHistoryPool::getMediaServerOnTimeSync(const QnSecurityCamResourcePtr &camera, qint64 timestampMs, QnTimePeriod* foundPeriod)
{
    NX_ASSERT(!camera.isNull(), Q_FUNC_INFO, "Camera resource is null!");
    if (camera.isNull())
        return QnMediaServerResourcePtr();

    updateCameraHistorySync(camera);
    return getMediaServerOnTime(camera, timestampMs, foundPeriod);
}

QnMediaServerResourcePtr QnCameraHistoryPool::getNextMediaServerAndPeriodOnTime(const QnSecurityCamResourcePtr &camera, qint64 timestamp, bool searchForward, QnTimePeriod* foundPeriod) const
{
    NX_ASSERT(!camera.isNull(), Q_FUNC_INFO, "Camera resource is null!");
    if (camera.isNull())
        return QnMediaServerResourcePtr();

    NX_ASSERT(foundPeriod, Q_FUNC_INFO, "target period MUST be present");
    if (!foundPeriod)
        return getMediaServerOnTime(camera, timestamp);

    foundPeriod->clear();

    QnMutexLocker lock(&m_mutex);

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

ec2::ApiCameraHistoryItemDataList QnCameraHistoryPool::getHistoryDetails(const QnUuid& cameraId, bool* isValid)
{
    QnMutexLocker lock(&m_mutex);
    *isValid = m_historyValidCameras.contains(cameraId) &&
        isValidHistoryDetails(cameraId, m_historyDetail.value(cameraId));
    return *isValid ? m_historyDetail.value(cameraId) : ec2::ApiCameraHistoryItemDataList();
}

bool QnCameraHistoryPool::isValidHistoryDetails(
    const QnUuid& cameraId,
    const ec2::ApiCameraHistoryItemDataList& historyDetails) const
{
    QnSecurityCamResourcePtr camera = toCamera(cameraId);
    if (!camera || camera->getStatus() != Qn::Recording)
        return true; //< nothing to check if no camera or not int recording state

    if (historyDetails.empty())
        return false;

    if (camera->getParentId() != historyDetails[historyDetails.size()-1].serverGuid)
        return false;

    return true;
}

bool QnCameraHistoryPool::testAndSetHistoryDetails(
    const QnUuid& cameraId,
    const ec2::ApiCameraHistoryItemDataList& historyDetails)
{
    QSet<QnUuid> serverList;
    for (const auto& value: historyDetails)
        serverList.insert(value.serverGuid);

    QnMutexLocker lock( &m_mutex );

    QSet<QnUuid> currentServerList;
    for (const auto& server: getCameraFootageDataUnsafe(cameraId, true))
        currentServerList.insert(server->getId());
    if (currentServerList != serverList)
        return false;

    if (!isValidHistoryDetails(cameraId, historyDetails))
        return false;

    m_historyDetail[cameraId] = historyDetails;
    m_historyValidCameras.insert(cameraId);

    lock.unlock();
    if (QnSecurityCamResourcePtr camera = toCamera(cameraId))
        emit cameraHistoryChanged(camera);
    return true;
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
        if (QnSecurityCamResourcePtr camera = toCamera(cameraId))
            emit cameraFootageChanged(camera);
}

void QnCameraHistoryPool::setServerFootageData(const QnUuid& serverGuid, const std::vector<QnUuid>& cameras) {
    {
        QnMutexLocker lock(&m_mutex);
        m_archivedCamerasByServer.insert(serverGuid, cameras);
    }
    for (const auto &cameraId : cameras) {
        invalidateCameraHistory(cameraId);
        if (QnSecurityCamResourcePtr camera = toCamera(cameraId))
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

QnSecurityCamResourceList QnCameraHistoryPool::getServerFootageCameras( const QnMediaServerResourcePtr &server ) const {
    QnSecurityCamResourceList result;
    if (!server)
        return result;

    for (const QnUuid &cameraId: getServerFootageData(server->getId())) {
        if (QnSecurityCamResourcePtr camera = toCamera(cameraId))
            result << camera;
    }

    return result;
}


bool QnCameraHistoryPool::updateCameraHistorySync(const QnSecurityCamResourcePtr &camera)
{
    if (!camera)
        return true;

    QnMutexLocker lock(&m_syncLoadMutex);
    while (m_syncRunningRequests.contains(camera->getId()))
        m_syncLoadWaitCond.wait(&m_syncLoadMutex);

    if (isCameraHistoryValid(camera))
        return true;

    StartResult result = updateCameraHistoryAsync(camera, [this, camera] (bool /*success*/)
    {
        QnMutexLocker lock(&m_syncLoadMutex);
        m_syncRunningRequests.remove(camera->getId());
        m_syncLoadWaitCond.wakeAll();
    });

    if (result == StartResult::started) {
        m_syncRunningRequests.insert(camera->getId());
        while (m_syncRunningRequests.contains(camera->getId()))
            m_syncLoadWaitCond.wait(&m_syncLoadMutex);
    }
    return result != StartResult::failed;
}
