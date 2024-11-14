// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <api/server_rest_connection_fwd.h>
#include <core/resource/resource_fwd.h>
#include <nx/utils/singleton.h>
#include <nx/utils/thread/mutex.h>
#include <nx/utils/thread/wait_condition.h>
#include <nx/utils/uuid.h>
#include <nx/vms/api/data/camera_history_data.h>
#include <nx/vms/common/system_context_aware.h>
#include <recording/time_period.h>

class QnCommonMessageProcessor;

/**
 *  Class for maintaining camera history - what server contains which part of the camera archive.
 *  Terms:
 *  \li Footage - part of the camera archive.
 *  \li Camera footage data - list of servers, that have this camera footage
 *  \li Server footage data - list of cameras, that have footage on this server
 *  \li Camera history - list of camera movements from one server to another.
 *  Main task for this class is to provide info, which server contains footage for the given
 *  camera for the given moment of time.
 *
 *  Footage data is stored in the database, so it is received and updated almost instantly.
 *  All methods that operates only footage data are reliable.
 *
 *  Camera history is not received instantly, so using signals is encouraged. It can also be
 *  changed in runtime, e.g. when server containing footage goes online or offline.
 *  All methods that require camera history to operate are non-reliable.
 *
 */
class NX_VMS_COMMON_API QnCameraHistoryPool:
    public QObject,
    public nx::vms::common::SystemContextAware
{
    Q_OBJECT

public:
    QnCameraHistoryPool(nx::vms::common::SystemContext* context, QObject* parent = nullptr);
    virtual ~QnCameraHistoryPool();

    /** Reset information about camera footage presence on different servers. */
    void resetServerFootageData(const nx::vms::api::ServerFootageDataList& serverFootageDataList);

    /** Update information about camera footage presence on a single server. */
    void setServerFootageData(const nx::vms::api::ServerFootageData& serverFootageData);

    /** Update information about camera footage presence on a single server. */
    void setServerFootageData(const nx::Uuid& serverGuid, const std::vector<nx::Uuid>& cameras);

    /** \return set of camera id's that have footage on the selected server. */
    QSet<nx::Uuid> getServerFootageData(const nx::Uuid& serverGuid) const;

    /** \return list of camera id's that have footage on the selected server. */
    QnVirtualCameraResourceList getServerFootageCameras(const QnMediaServerResourcePtr &server) const;

    /** \return server list where the selected camera has footage. */
    QnMediaServerResourceList getCameraFootageData(const nx::Uuid &cameraId, bool filterOnlineServers = false) const;

    /** \return server list where the selected camera has footage. */
    QnMediaServerResourceList getCameraFootageData(const QnVirtualCameraResourcePtr &camera, bool filterOnlineServers = false) const;

    /**
    * \return                       Server list where camera was archived during specified period.
    *                               If camera history still not loaded, full camera footage server
    *                               list will be returned.
    */
    QnMediaServerResourceList getCameraFootageData(const QnVirtualCameraResourcePtr &camera, const QnTimePeriod& timePeriod) const;

    /**
     * \return                      Server where camera was recorded on specified time.
     * \param camera                Camera to find
     * \param timestampMs           Timestamp in milliseconds to find
     * TODO: Fix params doc: foundPeriod and allowOfflineServers.
     * \param allowOfflineServers   Drop out offline media servers if parameter is false
     */
    QnMediaServerResourcePtr getMediaServerOnTime(const QnVirtualCameraResourcePtr &camera, qint64 timestampMs, QnTimePeriod* foundPeriod = 0) const;

    /**
     * \return                      Server where camera was recorded on specified time. This function could update cache in sync mode if it isn't up to date
     * \param camera                Camera to find
     * \param timestamp             Timestamp in milliseconds to find
     * \param allowOfflineServers   Drop out offline media servers if parameter is false
     */
    QnMediaServerResourcePtr getMediaServerOnTimeSync(const QnVirtualCameraResourcePtr &camera, qint64 timestampMs, QnTimePeriod* foundPeriod = 0);


    QnMediaServerResourcePtr getNextMediaServerAndPeriodOnTime(const QnVirtualCameraResourcePtr &camera, qint64 timestamp, bool searchForward, QnTimePeriod* foundPeriod) const;

    typedef std::function<void(bool success)> callbackFunction;
    enum class StartResult
    {
        ommited,    //< request doesn't start because of data is up to date, callback wont called.
        started,    //< async IO started success. callback will called.
        failed      //< async IO can't start. callback wont called.
    };
    /**
     * Update camera history for the given camera.
     * @param camera Target camera
     * @param callback Callback function that will be executed when request finished.
     * @return Status of the call.
     */
    StartResult updateCameraHistoryAsync(
        const QnVirtualCameraResourcePtr& camera,
        callbackFunction callback);

    /**
     * \brief                       Check if camera history is valid for the given camera.
     * \param camera                Target camera.
     * \returns                     True if the camera history for the given camera is already loaded, false otherwise.
     */
    bool isCameraHistoryValid(const QnVirtualCameraResourcePtr &camera) const;

    /**
     * Update camera history for the given camera. Function returns immediately if currently loaded
     * information is up to date
     * @param camera Target camera
     * @return True if no error, false otherwise.
     */
    bool updateCameraHistorySync(const QnVirtualCameraResourcePtr& camera);

    nx::vms::api::CameraHistoryItemDataList getHistoryDetails(const nx::Uuid& cameraId, bool* isValid);

    /**
     * \brief                       Set camera history details if provided server list in historyDetails still actual.
     *                              Otherwise don't change data and returns false.
     */
    bool testAndSetHistoryDetails(
        const nx::Uuid& cameraId,
        const nx::vms::api::CameraHistoryItemDataList& historyDetails);

    void setHistoryCheckDelay(int value);
    void setMessageProcessor(QnCommonMessageProcessor* messageProcessor);
signals:
    /**
     * \brief                       Notify that camera footage is changed - a server was added or removed or changed its status.
     *                              Usually it means that camera chunks must be updated (if needed).
     */
    void cameraFootageChanged(const QnVirtualCameraResourcePtr &camera);

    /**
     * \brief                       Notify that camera in no longer valid - a server was added or goes online.
     *                              Usually it means that camera history must be updated (if needed).
     */
    void cameraHistoryInvalidated(const QnVirtualCameraResourcePtr &camera);

    /**
     * \brief                       Notify about changes in the camera history.
     *                              Usually it means that all who use camera history should update their info.
     */
    void cameraHistoryChanged(const QnVirtualCameraResourcePtr &camera);

private:
    struct AsyncRequestInfo
    {
        nx::Uuid serverId;
        ::rest::Handle handle;

        bool operator==(const AsyncRequestInfo& other) const = default;
    };

    nx::vms::api::CameraHistoryItemDataList filterOnlineServers(
        const nx::vms::api::CameraHistoryItemDataList& dataList) const;

    QnVirtualCameraResourcePtr toCamera(const nx::Uuid& guid) const;
    QnMediaServerResourcePtr toMediaServer(const nx::Uuid& guid) const;

    void at_cameraPrepared(
        bool success,
        const AsyncRequestInfo& requestInfo,
        const nx::vms::api::CameraHistoryDataList& periods,
        callbackFunction callback);

    /** Mark camera history as dirty and subject to update. */
    void invalidateCameraHistory(const nx::Uuid &cameraId);

private:
    void checkCameraHistoryDelayed(QnVirtualCameraResourcePtr cam);
    QnMediaServerResourceList dtsCamFootageData(const QnVirtualCameraResourcePtr &camera
        , bool filterOnlineServers = false) const;
    QnMediaServerResourceList getCameraFootageDataUnsafe(
        const nx::Uuid &cameraId,
        bool filterOnlineServers = false) const;
    bool isValidHistoryDetails(
        const nx::Uuid& cameraId,
        const nx::vms::api::CameraHistoryItemDataList& historyDetails) const;
private:
    int m_historyCheckDelay;
    mutable nx::Mutex m_mutex;
    QMap<nx::Uuid, QSet<nx::Uuid>> m_archivedCamerasByServer; // archived cameras by server

    typedef QMap<nx::Uuid, nx::vms::api::CameraHistoryItemDataList> DetailHistoryMap;
    DetailHistoryMap m_historyDetail; // camera move detail by camera

    /** Set of cameras which have the data loaded and actual. */
    QSet<nx::Uuid> m_historyValidCameras;

    std::map<nx::Uuid, AsyncRequestInfo> m_asyncRunningRequests;

    QSet<nx::Uuid> m_syncRunningRequests;
    mutable nx::Mutex m_syncLoadMutex;
    nx::WaitCondition m_syncLoadWaitCond;
    QSet<nx::Uuid> m_camerasToCheck;
    QnCommonMessageProcessor* m_messageProcessor = nullptr;
};
