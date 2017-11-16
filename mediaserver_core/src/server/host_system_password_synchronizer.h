#pragma once

#include <QtCore/QObject>

#include <core/resource/resource_fwd.h>
#include <nx/utils/singleton.h>
#include <nx/utils/thread/mutex.h>
#include <nx/utils/safe_direct_connection.h>
#include <common/common_module_aware.h>

/**
 * Sets host system password to admin password if appropriate.
 * Currently, password is changed only on NX1/bananapi and only if OS is installed on HDD.
 */
class HostSystemPasswordSynchronizer:
    public QObject,
    public Singleton<HostSystemPasswordSynchronizer>,
    public QnCommonModuleAware,
    public Qn::EnableSafeDirectConnection
{
    Q_OBJECT

public:
    HostSystemPasswordSynchronizer(QnCommonModule* commonModule);
    virtual ~HostSystemPasswordSynchronizer() override;

    void syncLocalHostRootPasswordWithAdminIfNeeded(const QnUserResourcePtr& user);

private:
    QnMutex m_mutex;

    void setAdmin(QnUserResourcePtr admin);

private slots:
    void at_resourceFound(QnResourcePtr resource);
    void at_adminHashChanged(QnResourcePtr resource);
};
