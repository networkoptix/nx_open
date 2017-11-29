
#include <QtCore/QRunnable>
#include <QtCore/QThreadPool>

#include <nx/utils/log/log.h>

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
QnSyncTime::QnSyncTime(QObject *parent)
:
    QObject(parent),
    m_lastReceivedTime( 0 ),
    m_lastWarnTime( 0 ),
    m_lastLocalTime( 0 ),
    m_syncTimeRequestIssued( false ),
    m_refCounter( 1 )
{
    reset();
    m_timer.invalidate();
}

QnSyncTime::~QnSyncTime() {}


void QnSyncTime::updateTime(int /*reqID*/, ec2::ErrorCode errorCode, qint64 newTime)
{
    if( errorCode != ec2::ErrorCode::ok )
    {
        NX_LOG( lit("Failed to receive synchronized time: %1").arg(ec2::toString(errorCode)), cl_logWARNING );
        return;
    }

    QnMutexLocker lock( &m_mutex );
    qint64 oldTime = m_timer.isValid() ? m_lastReceivedTime + m_timer.elapsed() : newTime;

    m_lastReceivedTime = newTime;
    m_timer.restart();
    m_syncTimeRequestIssued = false;

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

std::chrono::microseconds QnSyncTime::currentTimePoint()
{
    return std::chrono::microseconds(currentUSecsSinceEpoch());
}

void QnSyncTime::reset()
{
    m_lastWarnTime = 0;
    m_lastLocalTime = 0;
}

unsigned int QnSyncTime::addRef()
{
    return ++m_refCounter;
}

unsigned int QnSyncTime::releaseRef()
{
    return --m_refCounter;
}

void* QnSyncTime::queryInterface(const nxpl::NX_GUID& interfaceID)
{
    if (memcmp(&interfaceID, &nxpl::IID_TimeProvider, sizeof(nxpl::IID_TimeProvider)) == 0)
    {
        addRef();
        return static_cast<nxpl::TimeProvider*>(this);
    }
    if (memcmp(&interfaceID, &nxpl::IID_PluginInterface, sizeof(nxpl::IID_PluginInterface)) == 0)
    {
        addRef();
        return static_cast<nxpl::PluginInterface*>(this);
    }
    return NULL;
}

uint64_t QnSyncTime::millisSinceEpoch() const
{
    return const_cast<QnSyncTime*>(this)->currentMSecsSinceEpoch();
}

void QnSyncTime::updateTime(qint64 newTime)
{
    updateTime( 0, ec2::ErrorCode::ok, newTime );
}

qint64 QnSyncTime::currentMSecsSinceEpoch()
{
    const qint64 localTime = QDateTime::currentMSecsSinceEpoch();
    QnMutexLocker lock( &m_mutex );

    if (
        (
            m_lastReceivedTime == 0
        ||  m_timer.elapsed() > EcTimeUpdatePeriod
        || qAbs(localTime-m_lastLocalTime) > EcTimeUpdatePeriod
        )
        && !m_syncTimeRequestIssued)
    {
        ec2::AbstractECConnectionPtr appServerConnection = QnAppServerConnectionFactory::ec2Connection();
        if( appServerConnection )
        {
            appServerConnection->getTimeManager(Qn::kSystemAccess)->getCurrentTime( this, (void(QnSyncTime::*)(int, ec2::ErrorCode, qint64))&QnSyncTime::updateTime );
            m_syncTimeRequestIssued = true;
        }
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
        return localTime;
}
