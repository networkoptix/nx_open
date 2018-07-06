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
    virtual ~QnSyncTime();

    qint64 currentMSecsSinceEpoch();
    qint64 currentUSecsSinceEpoch();
    std::chrono::microseconds currentTimePoint();
    QDateTime currentDateTime();

    void reset();

    virtual unsigned int addRef() override;
    virtual unsigned int releaseRef() override;
    virtual void* queryInterface(const nxpl::NX_GUID& interfaceID) override;
    virtual uint64_t millisSinceEpoch() const override;

public slots:
    //!Sets new synchronized time to \a newTime
    void updateTime(qint64 newTime);

signals:
    /**
     * This signal is emitted whenever time on Server changes. 
     */
    void timeChanged();

private:
    QElapsedTimer m_timer;
    qint64 m_lastReceivedTime;
    QnMutex m_mutex;
    qint64 m_lastWarnTime;
    qint64 m_lastLocalTime;
    bool m_syncTimeRequestIssued;
    std::atomic<unsigned int> m_refCounter;

private slots:
    //!Sets new synchronized time to \a newTime
    void updateTime(int reqID, ec2::ErrorCode errorCode, qint64 newTime);
};

#define qnSyncTime (QnSyncTime::instance())

#endif // QN_SYNC_TIME_H
