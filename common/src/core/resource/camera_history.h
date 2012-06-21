#ifndef QN_CAMERA_HISTORY_H
#define QN_CAMERA_HISTORY_H

#include <QtCore/QObject>

#include "recording/time_period.h"
#include "resource.h"
#include "resource_fwd.h"

struct QN_EXPORT QnCameraHistoryItem
{
    QnCameraHistoryItem(const QString& mac_, qint64 timestamp_, const QString& videoServerGuid_)
        : mac(mac_),
          timestamp(timestamp_),
          videoServerGuid(videoServerGuid_)
    {
    }

    QString mac;
    qint64 timestamp;
    QString videoServerGuid;
};
typedef QSharedPointer<QnCameraHistoryItem> QnCameraHistoryItemPtr;

struct QN_EXPORT QnCameraTimePeriod: QnTimePeriod
{
    QnCameraTimePeriod(qint64 startTimeMs, qint64 durationMs, QString serverGuid): QnTimePeriod(startTimeMs, durationMs), videoServerGuid(serverGuid) {}

    QnId getServerId() const;

    QString videoServerGuid;
};

typedef QList<QnCameraTimePeriod> QnCameraTimePeriodList;

class QN_EXPORT QnCameraHistory
{
public:
    QnCameraHistory() {}

    QString getMacAddress() const;
    void setMacAddress(const QString& macAddress);

    QnCameraTimePeriodList getTimePeriods() const;
    QnVideoServerResourcePtr getVideoServerOnTime(qint64 timestamp, bool searchForward, QnTimePeriod& currentPeriod);
    QnNetworkResourcePtr getCameraOnTime(qint64 timestamp, bool searchForward);
    QnVideoServerResourcePtr getNextVideoServerOnTime(qint64 timestamp, bool searchForward, QnTimePeriod& currentPeriod);
    QnNetworkResourceList getAllCamerasWithSameMac(const QnTimePeriod& timePeriod);
    QnNetworkResourceList getAllCamerasWithSameMac();

    void addTimePeriod(const QnCameraTimePeriod& period);
    qint64 getMinTime() const;

private:
    QnCameraTimePeriodList::const_iterator getVideoServerOnTimeItr(qint64 timestamp, bool searchForward);
    QnVideoServerResourcePtr getNextVideoServerFromTime(qint64 timestamp, QnTimePeriod& currentPeriod);
    QnVideoServerResourcePtr getPrevVideoServerFromTime(qint64 timestamp, QnTimePeriod& currentPeriod);

private:
    Q_DISABLE_COPY(QnCameraHistory);

    QnCameraTimePeriodList m_timePeriods;
    QString m_macAddress;
    mutable QMutex m_mutex;
};

typedef QSharedPointer<QnCameraHistory> QnCameraHistoryPtr;
typedef QList<QnCameraHistoryPtr> QnCameraHistoryList;

class QnCameraHistoryPool: public QObject {
    Q_OBJECT;
public:
    QnCameraHistoryPool(QObject *parent = NULL);
    virtual ~QnCameraHistoryPool();

    static QnCameraHistoryPool* instance();
    QnCameraHistoryPtr getCameraHistory(const QString& mac);
    void addCameraHistory(QnCameraHistoryPtr history);
    void addCameraHistoryItem(const QnCameraHistoryItem& historyItem);

    QnNetworkResourcePtr getCurrentCamera(const QnNetworkResourcePtr &resource);
    QnResourcePtr getCurrentCamera(const QnResourcePtr &resource);

    QnNetworkResourceList getAllCamerasWithSameMac(const QnNetworkResourcePtr &camera, const QnTimePeriod& timePeriod);
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
