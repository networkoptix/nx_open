
#include <QtCore/QRunnable>
#include <QtCore/QThreadPool>

#include <nx/utils/log/log.h>

#include "synctime.h"
#include "api/app_server_connection.h"
#include "api/session_manager.h"
//#include <nx/vms/time_sync/time_sync_manager.h>
#include <nx_ec/ec_api.h>

enum {
    EcTimeUpdatePeriod = 1000 * 60 * 5, /* 5 minutes. */
    TimeChangeThreshold = 1000 * 30,    /* 30 seconds. */
};


// -------------------------------------------------------------------------- //
// QnSyncTime
// -------------------------------------------------------------------------- //
QnSyncTime::QnSyncTime(QObject *parent): QObject(parent)
{
}

QDateTime QnSyncTime::currentDateTime() const
{
    return QDateTime::fromMSecsSinceEpoch(currentMSecsSinceEpoch());
}

qint64 QnSyncTime::currentUSecsSinceEpoch() const
{
    return currentMSecsSinceEpoch() * 1000;
}

std::chrono::microseconds QnSyncTime::currentTimePoint()
{
    return std::chrono::microseconds(currentUSecsSinceEpoch());
}

qint64 QnSyncTime::currentMSecsSinceEpoch() const
{
    QnMutexLocker lock(&m_mutex);

    ec2::AbstractECConnectionPtr appServerConnection = QnAppServerConnectionFactory::ec2Connection();
    return appServerConnection
        ? appServerConnection->timeSyncManager()->getSyncTime().count()
        : QDateTime::currentMSecsSinceEpoch();
}

void QnSyncTime::setTimeNotificationManager(ec2::AbstractTimeNotificationManagerPtr timeNotificationManager)
{
    QnMutexLocker lock(&m_mutex);

    if (m_timeNotificationManager)
        m_timeNotificationManager->disconnect(this);

    m_timeNotificationManager = timeNotificationManager;
    if (m_timeNotificationManager)
    {
        connect(
            m_timeNotificationManager.get(), &ec2::AbstractTimeNotificationManager::timeChanged,
            this, &QnSyncTime::timeChanged);
    }
}
