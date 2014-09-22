#ifndef QN_CAMERA_HISTORY_H
#define QN_CAMERA_HISTORY_H

#include <QtCore/QObject>
#include <QtCore/QMutex>
#include <QtCore/QUuid>

#include "recording/time_period.h"
#include "resource_fwd.h"

struct QnCameraHistoryItem
{
    QnCameraHistoryItem(const QString& cameraUniqueId, qint64 timestamp, const QUuid& mediaServerGuid)
        : cameraUniqueId(cameraUniqueId),
          timestamp(timestamp),
          mediaServerGuid(mediaServerGuid)
    {
    }

    QString cameraUniqueId;
    qint64 timestamp;
    QUuid mediaServerGuid;
};
typedef QSharedPointer<QnCameraHistoryItem> QnCameraHistoryItemPtr;

/*
struct QnCameraTimePeriod
{
    QnCameraTimePeriod(qint64 startTimeMs, qint64 durationMs, const QUuid& mediaServerGuid):
                       QnTimePeriod(startTimeMs, durationMs),
                       mediaServerGuid(mediaServerGuid) {}
    QUuid mediaServerGuid;
};
*/

//typedef QMap<qint64, QnCameraTimePeriod> QnCameraTimePeriodList;
typedef QMap<qint64, QUuid> QnServerHistoryMap; // key: timestamp, value: server ID

class QnCameraHistory
{
public:
    QnCameraHistory();

    QString getCameraUniqueId() const;
    void setCameraUniqueId(const QString &cameraUniqueId);

    QnMediaServerResourcePtr getMediaServerOnTime(qint64 timestamp, bool allowOfflineServers) const;
    QnMediaServerResourcePtr getNextMediaServerOnTime(qint64 timestamp, bool searchForward) const;
    QnMediaServerResourcePtr getNextMediaServerAndPeriodOnTime(qint64 timestamp, QnTimePeriod& currentPeriod, bool searchForward);

    QnMediaServerResourcePtr getMediaServerAndPeriodOnTime(qint64 timestamp, QnTimePeriod& currentPeriod, bool allowOfflineServers);

    QnMediaServerResourceList getAllCameraServers(const QnTimePeriod& timePeriod) const;
    QnMediaServerResourceList getAllCameraServers() const;

    /**
     * Exclude offline or disabled resources
     */
    QnMediaServerResourceList getOnlineCameraServers(const QnTimePeriod& timePeriod) const;

    void addTimePeriod(qint64 timestamp, const QUuid& serverId);
    void removeTimePeriod(const qint64 timestamp);
    qint64 getMinTime() const;

    QnServerHistoryMap getOnlineTimePeriods() const;
    void getItemsBefore(qint64 timestamp, const QUuid& serverId, QList<QnCameraHistoryItem>& result);
private:
    QnServerHistoryMap::const_iterator getMediaServerOnTimeItr(const QnServerHistoryMap& timePeriods, qint64 timestamp) const;
    QnMediaServerResourcePtr getNextMediaServerFromTime(const QnServerHistoryMap& timePeriods, qint64 timestamp) const;
    QnMediaServerResourcePtr getPrevMediaServerFromTime(const QnServerHistoryMap& timePeriods, qint64 timestamp) const;
    QnMediaServerResourcePtr getNextMediaServerAndPeriodFromTime(const QnServerHistoryMap& timePeriods, qint64 timestamp, QnTimePeriod& currentPeriod);
    QnMediaServerResourcePtr getPrevMediaServerAndPeriodFromTime(const QnServerHistoryMap& timePeriods, qint64 timestamp, QnTimePeriod& currentPeriod);
    QnMediaServerResourceList getAllCameraServersInternal(const QnTimePeriod& timePeriod, const QnServerHistoryMap cameraHistory) const;

private:
    Q_DISABLE_COPY(QnCameraHistory);

    QnServerHistoryMap m_fullTimePeriods;
    QString m_cameraUniqueId;
    mutable QMutex m_mutex;
};

Q_DECLARE_METATYPE(QnCameraHistoryList)
Q_DECLARE_METATYPE(QnCameraHistoryItemPtr)

class QnCameraHistoryPool: public QObject {
    Q_OBJECT;
public:
    QnCameraHistoryPool(QObject *parent = NULL);
    virtual ~QnCameraHistoryPool();

    static QnCameraHistoryPool* instance();
    QnCameraHistoryPtr getCameraHistory(const QnResourcePtr &camera) const;
    QnCameraHistoryPtr getCameraHistory(const QString& uniqueId) const;
    void addCameraHistory(const QnCameraHistoryPtr &history);
    void addCameraHistoryItem(const QnCameraHistoryItem& historyItem);

    void removeCameraHistoryItem(const QnCameraHistoryItem& historyItem);

    //QnNetworkResourcePtr getCurrentCamera(const QnNetworkResourcePtr &resource);
    //QnResourcePtr getCurrentCamera(const QnResourcePtr &resource);

    QnMediaServerResourceList getAllCameraServers(const QnNetworkResourcePtr &camera) const;
    QnMediaServerResourceList getAllCameraServers(const QnNetworkResourcePtr &camera, const QnTimePeriod& timePeriod) const;
    QnMediaServerResourceList getOnlineCameraServers(const QnNetworkResourcePtr &camera, const QnTimePeriod& timePeriod) const;
    QnMediaServerResourcePtr getMediaServerOnTime(const QnNetworkResourcePtr &camera, qint64 timestamp, bool allowOfflineServers) const;
    qint64 getMinTime(const QnNetworkResourcePtr &camera);

    //QList<QnCameraHistoryItem> getUnusedItems(const QMap<QString, qint64>& archiveMinTimes, const QUuid& serverId);
private:
    QnMediaServerResourceList getCurrentServer(const QnNetworkResourcePtr &camera) const;
private:
    typedef QMap<QString, QnCameraHistoryPtr> CameraHistoryMap; /* Map by camera unique id */
    CameraHistoryMap m_cameraHistory;
    mutable QMutex m_mutex;
};


#define qnHistoryPool (QnCameraHistoryPool::instance())

#endif // QN_CAMERA_HISTORY_H
