#ifndef QN_SYNC_TIME_H
#define QN_SYNC_TIME_H

#include <atomic>

#include <QtCore/QDateTime>
#include <nx/utils/thread/mutex.h>
#include <QtCore/QObject>

#include <nx_ec/ec_api.h>

#include <plugins/plugin_container_api.h>
#include <nx/utils/singleton.h>
#include <QtCore/QElapsedTimer>

namespace ec2 {
class AbstractTimeNotificationManager;
}

/** 
 * Time provider that is synchronized with Server.
 */
class QnSyncTime
:
    public QObject,
    public Singleton<QnSyncTime>,
    public nxpl::TimeProvider
{
    Q_OBJECT;

public:
    QnSyncTime(QObject *parent = NULL);
    void setTimeNotificationManager(ec2::AbstractTimeNotificationManagerPtr timeNotificationManager);

    qint64 currentMSecsSinceEpoch() const;
    qint64 currentUSecsSinceEpoch() const;
    std::chrono::microseconds currentTimePoint();
    QDateTime currentDateTime() const;

    virtual unsigned int addRef() override;
    virtual unsigned int releaseRef() override;
    virtual void* queryInterface(const nxpl::NX_GUID& interfaceID) override;
    virtual uint64_t millisSinceEpoch() const override;
signals:
    /**
     * This signal is emitted whenever time on Server changes. It is deprecated. Use TimeNotificationManager instead.
     */
    void timeChanged();
private:
    std::atomic<unsigned int> m_refCounter{0};
    ec2::AbstractTimeNotificationManagerPtr m_timeNotificationManager;
    mutable QnMutex m_mutex;
};

#define qnSyncTime (QnSyncTime::instance())

#endif // QN_SYNC_TIME_H
