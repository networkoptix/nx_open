#ifndef __CAMERA_HISTORY_H_
#define __CAMERA_HISTORY_H_

#include "recording/time_period.h"
#include "resource.h"
#include "resource_fwd.h"

struct QN_EXPORT QnCameraTimePeriod: QnTimePeriod
{
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
private:
    QnCameraTimePeriodList::const_iterator getVideoServerOnTimeItr(qint64 timestamp, bool searchForward);
    QnVideoServerResourcePtr getNextVideoServerFromTime(qint64 timestamp, QnTimePeriod& currentPeriod);
    QnVideoServerResourcePtr getPrevVideoServerFromTime(qint64 timestamp, QnTimePeriod& currentPeriod);
private:
    QnCameraTimePeriodList m_timePeriods;
    QString m_macAddress;
};

typedef QSharedPointer<QnCameraHistory> QnCameraHistoryPtr;

class QnCameraHistoryPool
{
public:
    static QnCameraHistoryPool* instance();
    QnCameraHistoryPtr getCameraHistory(const QString& mac);
    void addCameraHistory(const QString& mac, QnCameraHistoryPtr history);
private:
    typedef QMap<QString, QnCameraHistoryPtr> CameraHistoryMap;
    CameraHistoryMap m_cameraHistory;
};


#endif // __CAMERA_HISTORY_H_
