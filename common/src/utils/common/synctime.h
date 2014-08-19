#ifndef QN_SYNC_TIME_H
#define QN_SYNC_TIME_H

#include <QtCore/QDateTime>
#include <QtCore/QMutex>
#include <QtCore/QObject>

#include <nx_ec/ec_api.h>


/** 
 * Time provider that is synchronized with Server.
 */
class QnSyncTime: public QObject {
    Q_OBJECT;

public:
    QnSyncTime();
    virtual ~QnSyncTime();

    static QnSyncTime* instance();

    qint64 currentMSecsSinceEpoch();
    qint64 currentUSecsSinceEpoch();
    QDateTime currentDateTime();

    void reset();

public slots:
    //!Sets new synchronized time to \a newTime
    void updateTime(qint64 newTime);

signals:
    /**
     * This signal is emitted whenever time on Server changes. 
     */
    void timeChanged();

private:
    QTime m_timer;
    qint64 m_lastReceivedTime;
    QMutex m_mutex;
    qint64 m_lastWarnTime;
    qint64 m_lastLocalTime;
    bool m_syncTimeRequestIssued;

private slots:
    //!Sets new synchronized time to \a newTime
    void updateTime(int reqID, ec2::ErrorCode errorCode, qint64 newTime);
};

#define qnSyncTime (QnSyncTime::instance())

#endif // QN_SYNC_TIME_H
