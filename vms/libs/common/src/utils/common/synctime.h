#ifndef QN_SYNC_TIME_H
#define QN_SYNC_TIME_H

#include <atomic>

#include <QtCore/QDateTime>
#include <nx/utils/thread/mutex.h>
#include <QtCore/QObject>

#include <nx_ec/ec_api.h>

#include <nx/utils/singleton.h>
#include <QtCore/QElapsedTimer>

namespace ec2 {
class AbstractTimeNotificationManager;
}

/**
 * Time provider that is synchronized with Server.
 */
class QnSyncTime final
    :
    public QObject,
    public Singleton<QnSyncTime>
{
    Q_OBJECT;

public:
    QnSyncTime(QObject *parent = NULL);
    ~QnSyncTime();

    void setTimeNotificationManager(ec2::AbstractTimeNotificationManagerPtr timeNotificationManager);

    qint64 currentMSecsSinceEpoch() const;
    qint64 currentUSecsSinceEpoch() const;
    std::chrono::microseconds currentTimePoint();
    QDateTime currentDateTime() const;

signals:
    /**
     * Emitted whenever time on Server changes. Deprecated. Use TimeNotificationManager instead.
     */
    void timeChanged();

private:
    ec2::AbstractTimeNotificationManagerPtr m_timeNotificationManager;
    mutable QnMutex m_mutex;
};

#define qnSyncTime (QnSyncTime::instance())

#endif // QN_SYNC_TIME_H
