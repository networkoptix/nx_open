#ifndef QN_CAMERA_HISTORY_H
#define QN_CAMERA_HISTORY_H

#include <set>

#include <QtCore/QObject>
#include <QtCore/QMutex>

#include <core/resource/resource_fwd.h>

#include <nx_ec/data/api_fwd.h>

#include <recording/time_period.h>
#include <recording/time_period_list.h>

#include <utils/common/singleton.h>
#include <utils/common/uuid.h>

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

    /** \return server list where the selected camera has footage. */
    QnMediaServerResourceList getCameraFootageData(const QnVirtualCameraResourcePtr &camera) const;

    /*
    *   \return Server list where camera was archived during specified period.
    *   If camera history still not loaded, full camera footage server list will be returned.
    */
    QnMediaServerResourceList getCameraFootageData(const QnVirtualCameraResourcePtr &camera, const QnTimePeriod& timePeriod) const;

    /*
    *  \return media server where camera was recorded on specified time
    * @param camera     camera to find
    * @param timestamp     timestamp in milliseconds to find
    * @param allowOfflineServers  drop out offline media servers if parameter is false
    */
    QnMediaServerResourcePtr getMediaServerOnTime(const QnVirtualCameraResourcePtr &camera, qint64 timestampMs, QnTimePeriod* foundPeriod = 0) const;

    QnMediaServerResourcePtr getNextMediaServerAndPeriodOnTime(const QnVirtualCameraResourcePtr &camera, qint64 timestamp, bool searchForward, QnTimePeriod* foundPeriod) const;

    typedef std::function<void(bool success)> callbackFunction;
    bool prepareCamera(const QnVirtualCameraResourcePtr &camera, callbackFunction handler);

signals:
    /** Notify about changes in the list of servers where the camera has footage. */
    void cameraFootageChanged(const QnVirtualCameraResourcePtr &camera);

    /** Notify about changes in the camera history. */
    void cameraHistoryChanged(const QnVirtualCameraResourcePtr &camera);

private:
    QnMediaServerResourcePtr getCurrentServer(const QnVirtualCameraResourcePtr &camera) const;
    void setServerFootageDataNoLock(const QnUuid& serverGuid, const std::vector<QnUuid>& cameras);
    ec2::ApiCameraHistoryItemDataList::const_iterator getMediaServerOnTimeInternal(const ec2::ApiCameraHistoryItemDataList& detailData, qint64 timestamp) const;
    ec2::ApiCameraHistoryItemDataList filterOnlineServers(const ec2::ApiCameraHistoryItemDataList& dataList) const;
    QnMediaServerResourcePtr toMediaServer(const QnUuid& guid) const;

    bool isCameraDataLoaded(const QnVirtualCameraResourcePtr &camera) const;
    Q_SLOT void at_cameraPrepared(int status, const ec2::ApiCameraHistoryDataList &periods, int handle);
    
private:

    mutable QMutex m_mutex;
    QMap<QnUuid, std::vector<QnUuid>> m_archivedCamerasByServer; // archived cameras by server

    typedef QMap<QnUuid, ec2::ApiCameraHistoryItemDataList> DetailHistoryMap;
    DetailHistoryMap m_historyDetail; // camera move detail by camera

    typedef QMap<int, callbackFunction> HandlerMap;
    HandlerMap m_loadingCameras;
};


#define qnCameraHistoryPool (QnCameraHistoryPool::instance())

#endif // QN_CAMERA_HISTORY_H
