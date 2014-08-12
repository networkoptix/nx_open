
#include <QtCore/QRunnable>
#include <QtCore/QThreadPool>

#include <utils/common/log.h>

#include "synctime.h"
#include "api/app_server_connection.h"
#include "api/session_manager.h"

enum {
    EcTimeUpdatePeriod = 1000 * 60 * 5, /* 5 minutes. */
    TimeChangeThreshold = 1000 * 30,    /* 30 seconds. */
};


// -------------------------------------------------------------------------- //
// QnSyncTime
// -------------------------------------------------------------------------- //
QnSyncTime::QnSyncTime()
:
    m_lastReceivedTime( 0 ),
    m_lastWarnTime( 0 ),
    m_lastLocalTime( 0 ),
    m_syncTimeRequestIssued( false )
{
    reset();
}

void QnSyncTime::updateTime(int /*reqID*/, ec2::ErrorCode errorCode, qint64 newTime)
{
    if( errorCode != ec2::ErrorCode::ok )
    {
        NX_LOG( lit("Failed to receive synchronized time: %1").arg(ec2::toString(errorCode)), cl_logWARNING );
        return;
    }

    QMutexLocker lock(&m_mutex);
    qint64 oldTime = m_lastReceivedTime + m_timer.elapsed();
    
    m_lastReceivedTime = newTime;
    m_timer.restart();
    m_syncTimeRequestIssued = false;

    if(qAbs(oldTime - newTime) > TimeChangeThreshold)
    {
        lock.unlock();
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
    m_lastWarnTime = 0;
    m_lastLocalTime = 0;
}

qint64 QnSyncTime::currentMSecsSinceEpoch()
{
    QMutexLocker lock(&m_mutex);

    const qint64 localTime = QDateTime::currentMSecsSinceEpoch();
    if (
        (
            m_lastReceivedTime == 0 
        ||  m_timer.elapsed() > EcTimeUpdatePeriod 
        || qAbs(localTime-m_lastLocalTime) > EcTimeUpdatePeriod
        ) 
        && !m_syncTimeRequestIssued 
        && QnAppServerConnectionFactory::getConnection2())
    {
        QnAppServerConnectionFactory::getConnection2()->getCurrentTime( this, &QnSyncTime::updateTime );
        m_syncTimeRequestIssued = true;
    }
    m_lastLocalTime = localTime;

    if (m_lastReceivedTime) {
        qint64 time = m_lastReceivedTime + m_timer.elapsed();
        if (qAbs(time - localTime) > 1000 * 10 && localTime - m_lastWarnTime > 1000 * 10)
        {
            m_lastWarnTime = localTime;
            if (m_lastWarnTime == 0)
            {
                NX_LOG( lit("Local time differs from server's! local time %1, server time %2").
                    arg(QDateTime::fromMSecsSinceEpoch(localTime).toString()).arg(QDateTime::fromMSecsSinceEpoch(time).toString()), cl_logWARNING );
            }
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

