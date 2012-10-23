#ifndef QN_SYNC_TIME_H
#define QN_SYNC_TIME_H

#include <QtCore/QDateTime>
#include <QtCore/QMutex>

class QnSyncTimeTask;

/** 
 * Time provider that is synchronized with Enterprise Controller.
 */
class QnSyncTime {
public:
    QnSyncTime();

    static QnSyncTime *instance();
    qint64 currentMSecsSinceEpoch();
    qint64 currentUSecsSinceEpoch();

    QDateTime currentDateTime();
    
private:
    void updateTime(qint64 newTime);

private:
    QTime m_timer;
    qint64 m_lastReceivedTime;
    bool m_requestSended;
    QnSyncTimeTask* m_gotTimeTask;
    QMutex m_mutex;
    qint64 m_lastWarnTime;
    qint64 m_lastLocalTime;

    friend class QnSyncTimeTask;
};

#define qnSyncTime (QnSyncTime::instance())

#endif // QN_SYNC_TIME_H
