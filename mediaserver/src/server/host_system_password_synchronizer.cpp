/**********************************************************
* Jul 10, 2015
* a.kolesnikov@networkoptix.com
***********************************************************/

#include "host_system_password_synchronizer.h"

#include <QtCore/QMutexLocker>

#include <core/resource/resource.h>
#include <core/resource/user_resource.h>
#include <core/resource_management/resource_pool.h>
#include <utils/common/app_info.h>
#include <utils/crypt/linux_passwd_crypt.h>

#include "platform/platform_abstraction.h"
#include "platform/monitoring/platform_monitor.h"


HostSystemPasswordSynchronizer::HostSystemPasswordSynchronizer()
{
    connect(
        QnResourcePool::instance(), &QnResourcePool::resourceChanged,
        this, &HostSystemPasswordSynchronizer::at_adminUserChanged );
}

void HostSystemPasswordSynchronizer::syncLocalHostRootPasswordWithAdminIfNeeded( const QnUserResourcePtr& user )
{
#ifdef __linux__
    QMutexLocker lk( &m_mutex );

    if( QnAppInfo::armBox() == "bpi" || QnAppInfo::armBox() == "nx1" )
    {
        //#5785 changing root password on nx1 only if DB is located on HDD
        QList<QnPlatformMonitor::PartitionSpace> partitions =
            qnPlatform->monitor()->QnPlatformMonitor::totalPartitionSpaceInfo(
                QnPlatformMonitor::LocalDiskPartition );

        for( int i = 0; i < partitions.size(); ++i )
        {
            if( partitions[i].path == "/" )
            {
                if( partitions[i].devName.indexOf( lit( "rootfs" ) ) != -1 ||
                    partitions[i].devName == lit( "/dev/root" ) )
                {
                    return; //OS is on SD card or some internal memory
                }
                else
                {
                    break;  //can change root password
                }
            }
        }

        //changing root password in system
        if( !setRootPasswordDigest( "root", user->getCryptSha512Hash() ) )
        {
            qWarning() << "Failed to set root password on current system";
        }
    }
#else
    Q_UNUSED( user );
#endif
}

void HostSystemPasswordSynchronizer::at_adminUserChanged( const QnResourcePtr& resource )
{
    QnUserResourcePtr user = resource.dynamicCast<QnUserResource>();
    if( !user )
        return;

    if( !user->isAdmin() )
        return;

    syncLocalHostRootPasswordWithAdminIfNeeded( user );
}
