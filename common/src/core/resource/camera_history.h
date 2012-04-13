#ifndef __CAMERA_HISTORY_H_
#define __CAMERA_HISTORY_H_

#include "recording/time_period.h"
#include "resource.h"
#include "resource_fwd.h"

struct QN_EXPORT QnCameraTimePeriod: QnTimePeriod
{
    QnCameraTimePeriod(qint64 startTimeMs, qint64 durationMs, QnId serverId): QnTimePeriod(startTimeMs, durationMs), videoServerId(serverId) {}

    QnId videoServerId;
};

typedef QList<QnCameraTimePeriod> QnCameraTimePeriodList;

class QN_EXPORT QnCameraHistory
{
public:
    QString getMacAddress() const;
    void setMacAddress(const QString& macAddress);

    QnCameraTimePeriodList getTimePeriods() const;
    QnVideoServerResourcePtr getVideoServerOnTime(qint64 timestamp, bool searchForward, QnTimePeriod& currentPeriod);
    QnVideoServerResourcePtr getNextVideoServerOnTime(qint64 timestamp, bool searchForward, QnTimePeriod& currentPeriod);
    QList<QnNetworkResourcePtr> getAllCamerasWithSameMac(const QnTimePeriod& timePeriod);

    void addTimePeriod(const QnCameraTimePeriod& period);
    qint64 getMinTime() const;
private:
    QnCameraTimePeriodList::const_iterator getVideoServerOnTimeItr(qint64 timestamp, bool searchForward);
    QnVideoServerResourcePtr getNextVideoServerFromTime(qint64 timestamp, QnTimePeriod& currentPeriod);
    QnVideoServerResourcePtr getPrevVideoServerFromTime(qint64 timestamp, QnTimePeriod& currentPeriod);
private:
    QnCameraTimePeriodList m_timePeriods;
    QString m_macAddress;
    mutable QMutex m_mutex;
};

typedef QSharedPointer<QnCameraHistory> QnCameraHistoryPtr;

class QnCameraHistoryPool
{
public:
    static QnCameraHistoryPool* instance();
    QnCameraHistoryPtr getCameraHistory(const QString& mac);
    void addCameraHistory(QnCameraHistoryPtr history);

    QList<QnNetworkResourcePtr> getAllCamerasWithSameMac(QnNetworkResourcePtr camera, const QnTimePeriod& timePeriod);
    qint64 getMinTime(QnNetworkResourcePtr camera);
private:
    typedef QMap<QString, QnCameraHistoryPtr> CameraHistoryMap;
    CameraHistoryMap m_cameraHistory;
    mutable QMutex m_mutex;
};


#endif // __CAMERA_HISTORY_H_
