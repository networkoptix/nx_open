#ifndef __SYNC_TIME_H__
#define __SYNC_TIME_H__

#include <QDateTime>

/** 
* Time synchronized with Enterprise Controller
*/

class QnSyncTimeTask;

class QnSyncTime
{
public:
    QnSyncTime();

    static QnSyncTime* instance();
    qint64 currentMSecsSinceEpoch();

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

    friend class QnSyncTimeTask;
};

#define qnSyncTime QnSyncTime::instance()

#endif // __SYNC_TIME_H__
