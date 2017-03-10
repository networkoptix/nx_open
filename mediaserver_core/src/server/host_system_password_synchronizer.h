#pragma once

#include <QtCore/QObject>

#include <core/resource/resource_fwd.h>
#include <nx/utils/singleton.h>
#include <nx/utils/thread/mutex.h>
#include <utils/common/safe_direct_connection.h>


//!Sets host system password to admin password if appropriate
/*!
    Currently, password is changed only on NX1/bananapi and only if OS is installed on HDD drive
*/
class HostSystemPasswordSynchronizer
:
    public QObject,
    public Singleton<HostSystemPasswordSynchronizer>,
    public Qn::EnableSafeDirectConnection
{
    Q_OBJECT
public:
    HostSystemPasswordSynchronizer();
    virtual ~HostSystemPasswordSynchronizer() override;

    void syncLocalHostRootPasswordWithAdminIfNeeded(const QnUserResourcePtr& user);

private:
    QnMutex m_mutex;

private:
    void setAdmin(QnUserResourcePtr admin);

private slots:
    void at_resourseFound(QnResourcePtr resource);
    void at_adminHashChanged(QnResourcePtr resource);
};
