#include <QtCore/QRunnable>
#include <QtCore/QThreadPool>

#include "synctime.h"
#include "api/app_server_connection.h"
#include "api/session_manager.h"

enum {
    EcTimeUpdatePeriod = 1000 * 60 * 5, /* 5 minutes. */
    TimeChangeThreshold = 1000 * 30,    /* 30 seconds. */
};


// -------------------------------------------------------------------------- //
// QnSyncTimeTask
// -------------------------------------------------------------------------- //
class QnSyncTimeTask: public QRunnable {
public:
    QnSyncTimeTask(QnSyncTime* owner): m_owner(owner) {}

    void run() {
        ec2::AbstractECConnectionPtr appServerConnection = QnAppServerConnectionFactory::getConnection2();
        qint64 time;
        if (appServerConnection && appServerConnection->getCurrentTimeSync(&time) == ec2::ErrorCode::ok) 
            m_owner->updateTime(time);
    }

private:
    QnSyncTime* m_owner;
    QUrl m_url;
};


// -------------------------------------------------------------------------- //
// QnSyncTime
// -------------------------------------------------------------------------- //
QnSyncTime::QnSyncTime()
{
    m_lastReceivedTime = 0;
    reset();
}

void QnSyncTime::updateTime(qint64 newTime)
{
    QMutexLocker lock(&m_mutex);
    qint64 oldTime = m_lastReceivedTime + m_timer.elapsed();
    
    m_lastReceivedTime = newTime;
    m_timer.restart();
    m_gotTimeTask = 0;

    if(qAbs(oldTime - newTime) > TimeChangeThreshold)
    {
        lock.unlock();  //to avoid deadlock: in case if timeChanged signal handler will call thread-safe method of this class
        emit timeChanged();
    }
}

QDateTime QnSyncTime::currentDateTime()
{
    return QDateTime::fromMSecsSinceEpoch(currentMSecsSinceEpoch());
}

qint64 QnSyncTime::currentUSecsSinceEpoch()
{
    return currentMSecsSinceEpoch() * 1000;
}

void QnSyncTime::reset()
{
    m_gotTimeTask = 0;
    m_lastWarnTime = 0;
    m_lastLocalTime = 0;
}

qint64 QnSyncTime::currentMSecsSinceEpoch()
{
    QMutexLocker lock(&m_mutex);
    qint64 localTime = QDateTime::currentMSecsSinceEpoch();
    if( (m_lastReceivedTime == 0 || m_timer.elapsed() > EcTimeUpdatePeriod || qAbs(localTime-m_lastLocalTime) > EcTimeUpdatePeriod)
        && m_gotTimeTask == 0
        && QnSessionManager::instance()->isReady()
        && !QnAppServerConnectionFactory::defaultUrl().isEmpty() )
    {
        m_gotTimeTask = new QnSyncTimeTask(this);
        m_gotTimeTask->setAutoDelete(true);
        QThreadPool::globalInstance()->start(m_gotTimeTask);
    }
    m_lastLocalTime = localTime;

    if (m_lastReceivedTime) {
        qint64 time = m_lastReceivedTime + m_timer.elapsed();
        if (qAbs(time - localTime) > 1000 * 10 && localTime - m_lastWarnTime > 1000 * 10)
        {
            m_lastWarnTime = localTime;
            if (m_lastWarnTime == 0)
                qWarning() << "Local time differs from enterprise controller! local time=" << QDateTime::fromMSecsSinceEpoch(localTime).toString() <<
                            "EC time=" << QDateTime::fromMSecsSinceEpoch(time).toString();
        }
        return time;
    }
    else
        return QDateTime::currentMSecsSinceEpoch();
}

Q_GLOBAL_STATIC(QnSyncTime, qn_syncTime_instance);

QnSyncTime* QnSyncTime::instance()
{
    return qn_syncTime_instance();
}

