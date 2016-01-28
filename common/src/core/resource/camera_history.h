#ifndef QN_CAMERA_HISTORY_H
#define QN_CAMERA_HISTORY_H

#include <set>

#include <QtCore/QObject>
#include <utils/thread/mutex.h>

#include <core/resource/resource_fwd.h>

#include <nx_ec/data/api_fwd.h>

#include <recording/time_period.h>
#include <recording/time_period_list.h>

#include <utils/common/singleton.h>
#include <utils/common/uuid.h>
#include "api/server_rest_connection_fwd.h"

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
class QnCameraHistoryPool: public QObject, public Singleton<QnCameraHistoryPool> {
    Q_OBJECT

public:
    QnCameraHistoryPool(QObject *parent = NULL);
    virtual ~QnCameraHistoryPool();

    /** Reset information about camera footage presence on different servers. */
    void resetServerFootageData(const ec2::ApiServerFootageDataList& serverFootageDataList);

    /** Update information about camera footage presence on a single server. */
    void setServerFootageData(const ec2::ApiServerFootageData &serverFootageData);

    /** Update information about camera footage presence on a single server. */
    void setServerFootageData(const QnUuid& serverGuid, const std::vector<QnUuid>& cameras);

    /** \return list of camera id's that have footage on the selected server. */
    std::vector<QnUuid> getServerFootageData(const QnUuid& serverGuid) const;

    /** \return list of camera id's that have footage on the selected server. */
    QnVirtualCameraResourceList getServerFootageCameras(const QnMediaServerResourcePtr &server) const;

    /** \return server list where the selected camera has footage. */
    QnMediaServerResourceList getCameraFootageData(const QnUuid &cameraId, bool filterOnlineServers = false) const;

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
     * \param timestamp             Timestamp in milliseconds to find
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
     * \param camera                Target camera
     * \param callback              Callback function that will be executed when request finished.
     * \returns                     True if request was successful, false otherwise.
     */
    StartResult updateCameraHistoryAsync(const QnVirtualCameraResourcePtr &camera, callbackFunction callback);

    /**
     * \brief                       Check if camera history is valid for the given camera.
     * \param camera                Target camera.
     * \returns                     True if the camera history for the given camera is already loaded, false otherwise.
     */
    bool isCameraHistoryValid(const QnVirtualCameraResourcePtr &camera) const;

    /**
     * \brief                       Update camera history for the given camera. Function returns immediately if currently loaded information is up to date
     * \param camera                Target camera.
     * \returns                     True if no error.
     */
    bool updateCameraHistorySync(const QnVirtualCameraResourcePtr &camera);
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
    ec2::ApiCameraHistoryItemDataList filterOnlineServers(const ec2::ApiCameraHistoryItemDataList& dataList) const;

    QnVirtualCameraResourcePtr toCamera(const QnUuid& guid) const;
    QnMediaServerResourcePtr toMediaServer(const QnUuid& guid) const;

    void at_cameraPrepared(bool success, const rest::Handle& requestId, const ec2::ApiCameraHistoryDataList &periods, callbackFunction callback);

    /** Mark camera history as dirty and subject to update. */
    void invalidateCameraHistory(const QnUuid &cameraId);

private:
    void checkCameraHistoryDelayed(QnVirtualCameraResourcePtr cam);
    QnMediaServerResourceList dtsCamFootageData(const QnVirtualCameraResourcePtr &camera
        , bool filterOnlineServers = false) const;
private:

    mutable QnMutex m_mutex;
    QMap<QnUuid, std::vector<QnUuid>> m_archivedCamerasByServer; // archived cameras by server

    typedef QMap<QnUuid, ec2::ApiCameraHistoryItemDataList> DetailHistoryMap;
    DetailHistoryMap m_historyDetail; // camera move detail by camera

    /** Set of cameras which have the data loaded and actual. */
    QSet<QnUuid> m_historyValidCameras;

    //typedef QMap<int, callbackFunction> HandlerMap;
    QMap<QnUuid, rest::Handle> m_asyncRunningRequests;
    QSet<QnUuid> m_syncRunningRequests;
    mutable QnMutex m_syncLoadMutex;
    QnWaitCondition m_syncLoadWaitCond;
    QSet<QnUuid> m_camerasToCheck;
};


#define qnCameraHistoryPool (QnCameraHistoryPool::instance())

#endif // QN_CAMERA_HISTORY_H
