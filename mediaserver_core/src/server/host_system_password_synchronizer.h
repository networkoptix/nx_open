/**********************************************************
* Jul 10, 2015
* a.kolesnikov@networkoptix.com
***********************************************************/

#ifndef HOST_SYSTEM_PASSWORD_SYNCHRONIZER_H
#define HOST_SYSTEM_PASSWORD_SYNCHRONIZER_H

#include <QtCore/QMutex>
#include <QtCore/QObject>

#include <core/resource/resource_fwd.h>
#include <utils/common/singleton.h>


//!Sets host system password to admin password if appropriate
/*!
    Currently, password is changed only on NX1/bananapi and only if OS is installed on HDD drive
*/
class HostSystemPasswordSynchronizer
:
    public QObject,
    public Singleton<HostSystemPasswordSynchronizer>
{
public:
    HostSystemPasswordSynchronizer();

    void syncLocalHostRootPasswordWithAdminIfNeeded( const QnUserResourcePtr& user );

private:
    QMutex m_mutex;

private slots:
    void at_adminUserChanged( const QnResourcePtr& resource );
};

#endif  //HOST_SYSTEM_PASSWORD_SYNCHRONIZER_H
