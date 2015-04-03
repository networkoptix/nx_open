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

    void setCamerasWithArchiveList(const ec2::ApiCameraHistoryDataList& cameraHistoryList);
    void setCamerasWithArchive(const QnUuid& serverGuid, const std::vector<QnUuid>& cameras);
    std::vector<QnUuid> getCamerasWithArchive(const QnUuid& serverGuid) const;


    typedef std::function<void(bool success)> callbackFunction;
    bool prepareCamera(const QnVirtualCameraResourcePtr &camera, callbackFunction handler);

    /*
    *   \return Server list where camera were archived.
    */
    QnMediaServerResourceList getFootageServersByCamera(const QnVirtualCameraResourcePtr &camera) const;

    /*
    *   \return Server list where camera was archived during specified period - if data is loaded.
    */
    QnMediaServerResourceList tryGetFootageServersByCameraPeriod(const QnVirtualCameraResourcePtr &camera, const QnTimePeriod& timePeriod) const;

    /*
    *  \return media server where camera was recorded on specified time
    * @param camera     camera to find
    * @param timestamp     timestamp in milliseconds to find
    * @param allowOfflineServers  drop out offline media servers if parameter is false
    */
    QnMediaServerResourcePtr getMediaServerOnTime(const QnVirtualCameraResourcePtr &camera, qint64 timestampMs, QnTimePeriod* foundPeriod = 0) const;

    QnMediaServerResourcePtr getNextMediaServerAndPeriodOnTime(const QnVirtualCameraResourcePtr &camera, qint64 timestamp, bool searchForward, QnTimePeriod* foundPeriod) const;
private:
    QnMediaServerResourcePtr getCurrentServer(const QnVirtualCameraResourcePtr &camera) const;
    void setCamerasWithArchiveNoLock(const QnUuid& serverGuid, const std::vector<QnUuid>& cameras);
    ec2::ApiCameraHistoryMoveDataList::const_iterator getMediaServerOnTimeInternal(const ec2::ApiCameraHistoryMoveDataList& detailData, qint64 timestamp) const;
    ec2::ApiCameraHistoryMoveDataList filterOnlineServers(const ec2::ApiCameraHistoryMoveDataList& dataList) const;
    QnMediaServerResourcePtr toMediaServer(const QnUuid& guid) const;

    bool isCameraDataLoaded(const QnVirtualCameraResourcePtr &camera) const;
    Q_SLOT void at_cameraPrepared(int status, const ec2::ApiCameraHistoryDetailDataList &periods, int handle);
    
private:

    mutable QMutex m_mutex;
    QMap<QnUuid, std::vector<QnUuid>> m_archivedCamerasByServer; // archived cameras by server

    typedef QMap<QnUuid, ec2::ApiCameraHistoryMoveDataList> DetailHistoryMap;
    DetailHistoryMap m_historyDetail; // camera move detail by camera

    typedef QMap<int, callbackFunction> HandlerMap;
    HandlerMap m_loadingCameras;
};


#define qnCameraHistoryPool (QnCameraHistoryPool::instance())

#endif // QN_CAMERA_HISTORY_H
