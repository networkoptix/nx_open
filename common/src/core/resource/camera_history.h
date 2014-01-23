#ifndef QN_CAMERA_HISTORY_H
#define QN_CAMERA_HISTORY_H

#include <QtCore/QObject>

#include "recording/time_period.h"
#include "resource.h"
#include "resource_fwd.h"

struct QN_EXPORT QnCameraHistoryItem
{
    QnCameraHistoryItem(const QString& physicalId_, qint64 timestamp_, const QString& mediaServerGuid_)
        : physicalId(physicalId_),
          timestamp(timestamp_),
          mediaServerGuid(mediaServerGuid_)
    {
    }

    QString physicalId;
    qint64 timestamp;
    QString mediaServerGuid;
};
typedef QSharedPointer<QnCameraHistoryItem> QnCameraHistoryItemPtr;

struct QN_EXPORT QnCameraTimePeriod: QnTimePeriod
{
    QnCameraTimePeriod(qint64 startTimeMs, qint64 durationMs, QString serverGuid): QnTimePeriod(startTimeMs, durationMs), mediaServerGuid(serverGuid) {}

    QnId getServerId() const;

    QString mediaServerGuid;
};

typedef QList<QnCameraTimePeriod> QnCameraTimePeriodList;

class QN_EXPORT QnCameraHistory
{
public:
    QnCameraHistory();

    QString getPhysicalId() const;
    void setPhysicalId(const QString& physicalId);

    QnMediaServerResourcePtr getMediaServerOnTime(qint64 timestamp, bool searchForward, QnTimePeriod& currentPeriod, bool gotOfflineCameras);
    QnNetworkResourcePtr getCameraOnTime(qint64 timestamp, bool searchForward, bool gotOfflineCameras);
    QnMediaServerResourcePtr getNextMediaServerOnTime(qint64 timestamp, bool searchForward, QnTimePeriod& currentPeriod);
    QnNetworkResourceList getAllCamerasWithSamePhysicalId(const QnTimePeriod& timePeriod);
    QnNetworkResourceList getAllCamerasWithSamePhysicalId();

    /**
     * Exclude offline or disabled resources
     */
    QnNetworkResourceList getOnlineCamerasWithSamePhysicalId(const QnTimePeriod& timePeriod);

    void addTimePeriod(const QnCameraTimePeriod& period);
    qint64 getMinTime() const;

    QnCameraTimePeriodList getOnlineTimePeriods() const;

private:
    QnCameraTimePeriodList::const_iterator getMediaServerOnTimeItr(const QnCameraTimePeriodList& timePeriods, qint64 timestamp, bool searchForward);
    QnMediaServerResourcePtr getNextMediaServerFromTime(const QnCameraTimePeriodList& timePeriods, qint64 timestamp, QnTimePeriod& currentPeriod);
    QnMediaServerResourcePtr getPrevMediaServerFromTime(const QnCameraTimePeriodList& timePeriods, qint64 timestamp, QnTimePeriod& currentPeriod);
    QnNetworkResourceList getCamerasWithSamePhysicalIdInternal(const QnTimePeriod& timePeriod, const QnCameraTimePeriodList cameraHistory);

private:
    Q_DISABLE_COPY(QnCameraHistory);

    QnCameraTimePeriodList m_fullTimePeriods;
    QString m_physicalId;
    mutable QMutex m_mutex;
};


typedef QSharedPointer<QnCameraHistory> QnCameraHistoryPtr;
typedef QList<QnCameraHistoryPtr> QnCameraHistoryList;
typedef QSharedPointer<QnCameraHistoryList> QnCameraHistoryListPtr;

Q_DECLARE_METATYPE(QnCameraHistoryList)

class QnCameraHistoryPool: public QObject {
    Q_OBJECT;
public:
    QnCameraHistoryPool(QObject *parent = NULL);
    virtual ~QnCameraHistoryPool();

    static QnCameraHistoryPool* instance();
    QnCameraHistoryPtr getCameraHistory(const QString& physicalId);
    void addCameraHistory(QnCameraHistoryPtr history);
    void addCameraHistoryItem(const QnCameraHistoryItem& historyItem);

    //QnNetworkResourcePtr getCurrentCamera(const QnNetworkResourcePtr &resource);
    //QnResourcePtr getCurrentCamera(const QnResourcePtr &resource);

    QnNetworkResourceList getAllCamerasWithSamePhysicalId(const QnNetworkResourcePtr &camera);
    QnNetworkResourceList getAllCamerasWithSamePhysicalId(const QnNetworkResourcePtr &camera, const QnTimePeriod& timePeriod);
    QnNetworkResourceList getOnlineCamerasWithSamePhysicalId(const QnNetworkResourcePtr &camera, const QnTimePeriod& timePeriod);
    qint64 getMinTime(QnNetworkResourcePtr camera);

signals:
    void currentCameraChanged(const QnNetworkResourcePtr &camera);

private:
    typedef QMap<QString, QnCameraHistoryPtr> CameraHistoryMap;
    CameraHistoryMap m_cameraHistory;
    mutable QMutex m_mutex;
};


#define qnHistoryPool (QnCameraHistoryPool::instance())

#endif // QN_CAMERA_HISTORY_H
