#include <QtCore/QRunnable>
#include <QtCore/QThreadPool>

#include "synctime.h"
#include "api/app_server_connection.h"
#include "api/session_manager.h"

static const int SYNC_TIME_INTERVAL = 1000 * 60 * 5;

class QnSyncTimeTask: public QRunnable {
public:
    QnSyncTimeTask(QnSyncTime* owner): m_owner(owner) {}
    void run()
    {
        QnAppServerConnectionPtr appServerConnection = QnAppServerConnectionFactory::createConnection();
        qint64 rez = appServerConnection->getCurrentTime();
        if (rez > 0) 
            m_owner->updateTime(rez);
    }

private:
    QnSyncTime* m_owner;
    QUrl m_url;
};


// ------------------------------ QnSyncTime ------------------------------

QnSyncTime::QnSyncTime()
{
    m_lastReceivedTime = 0;
    m_gotTimeTask = 0;
    m_lastWarnTime = 0;
}

void QnSyncTime::updateTime(qint64 newTime)
{
    QMutexLocker lock(&m_mutex);
    m_lastReceivedTime = newTime;
    m_timer.restart();
    m_gotTimeTask = 0;
}

QDateTime QnSyncTime::currentDateTime()
{
    return QDateTime::fromMSecsSinceEpoch(currentMSecsSinceEpoch());
}

qint64 QnSyncTime::currentUSecsSinceEpoch()
{
    return currentMSecsSinceEpoch() * 1000;
}

qint64 QnSyncTime::currentMSecsSinceEpoch()
{
    QMutexLocker lock(&m_mutex);

    if ((m_lastReceivedTime == 0 || m_timer.elapsed() > SYNC_TIME_INTERVAL) && m_gotTimeTask == 0 && QnSessionManager::instance()->isReady())
    {
        m_gotTimeTask = new QnSyncTimeTask(this);
        QThreadPool::globalInstance()->start(m_gotTimeTask);
    }
    if (m_lastReceivedTime) {
        qint64 time = m_lastReceivedTime + m_timer.elapsed();
        qint64 localTime = QDateTime::currentMSecsSinceEpoch();
        if (qAbs(time - localTime) > 1000 * 10 && localTime - m_lastWarnTime > 1000*10)
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

Q_GLOBAL_STATIC(QnSyncTime, getInstance);

QnSyncTime* QnSyncTime::instance()
{
    return getInstance();
}
