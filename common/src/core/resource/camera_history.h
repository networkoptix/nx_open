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

struct QnCameraTimePeriod: QnTimePeriod
{
    QnCameraTimePeriod(qint64 startTimeMs, qint64 durationMs, const QUuid& mediaServerGuid):
        QnTimePeriod(startTimeMs, durationMs),
        mediaServerGuid(mediaServerGuid) {}
    QUuid mediaServerGuid;
};

typedef QList<QnCameraTimePeriod> QnCameraTimePeriodList;

class QnCameraHistory
{
public:
    QnCameraHistory();

    QString getCameraUniqueId() const;
    void setCameraUniqueId(const QString &cameraUniqueId);

    QnMediaServerResourcePtr getMediaServerOnTime(qint64 timestamp, bool searchForward, bool allowOfflineServers) const;
    QnMediaServerResourcePtr getNextMediaServerOnTime(qint64 timestamp, bool searchForward) const;
    QnMediaServerResourcePtr getLastMediaServer() const;

    QnMediaServerResourcePtr getMediaServerAndPeriodOnTime(qint64 timestamp, bool searchForward, QnTimePeriod& currentPeriod, bool allowOfflineServers);
    QnMediaServerResourcePtr getNextMediaServerAndPeriodOnTime(qint64 timestamp, bool searchForward, QnTimePeriod& currentPeriod);

    QnMediaServerResourceList getAllCameraServers(const QnTimePeriod& timePeriod) const;
    QnMediaServerResourceList getAllCameraServers() const;

    /**
     * Exclude offline or disabled resources
     */
    QnMediaServerResourceList getOnlineCameraServers(const QnTimePeriod& timePeriod) const;

    void addTimePeriod(const QnCameraTimePeriod& period);
    qint64 getMinTime() const;

    QnCameraTimePeriodList getOnlineTimePeriods() const;
private:
    QnCameraTimePeriodList::const_iterator getMediaServerOnTimeItr(const QnCameraTimePeriodList& timePeriods, qint64 timestamp, bool searchForward) const;
    QnMediaServerResourcePtr getNextMediaServerFromTime(const QnCameraTimePeriodList& timePeriods, qint64 timestamp) const;
    QnMediaServerResourcePtr getPrevMediaServerFromTime(const QnCameraTimePeriodList& timePeriods, qint64 timestamp) const;
    QnMediaServerResourcePtr getNextMediaServerAndPeriodFromTime(const QnCameraTimePeriodList& timePeriods, qint64 timestamp, QnTimePeriod& currentPeriod);
    QnMediaServerResourcePtr getPrevMediaServerAndPeriodFromTime(const QnCameraTimePeriodList& timePeriods, qint64 timestamp, QnTimePeriod& currentPeriod);
    QnMediaServerResourceList getAllCameraServersInternal(const QnTimePeriod& timePeriod, const QnCameraTimePeriodList cameraHistory) const;

private:
    Q_DISABLE_COPY(QnCameraHistory);

    QnCameraTimePeriodList m_fullTimePeriods;
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
    void addCameraHistory(const QnCameraHistoryPtr &history);
    void addCameraHistoryItem(const QnCameraHistoryItem& historyItem);

    //QnNetworkResourcePtr getCurrentCamera(const QnNetworkResourcePtr &resource);
    //QnResourcePtr getCurrentCamera(const QnResourcePtr &resource);

    QnMediaServerResourceList getAllCameraServers(const QnNetworkResourcePtr &camera) const;
    QnMediaServerResourceList getAllCameraServers(const QnNetworkResourcePtr &camera, const QnTimePeriod& timePeriod) const;
    QnMediaServerResourceList getOnlineCameraServers(const QnNetworkResourcePtr &camera, const QnTimePeriod& timePeriod) const;
    QnMediaServerResourcePtr getLastMediaServer(const QnNetworkResourcePtr &camera) const;
    qint64 getMinTime(const QnNetworkResourcePtr &camera);

signals:
    void currentCameraChanged(const QnNetworkResourcePtr &camera);
private:
    QnMediaServerResourceList getCurrentServer(const QnNetworkResourcePtr &camera) const;
private:
    typedef QMap<QString, QnCameraHistoryPtr> CameraHistoryMap; /* Map by camera unique id */
    CameraHistoryMap m_cameraHistory;
    mutable QMutex m_mutex;
};


#define qnHistoryPool (QnCameraHistoryPool::instance())

#endif // QN_CAMERA_HISTORY_H
