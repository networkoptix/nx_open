#include "resource_notification_manager.h"

#include <core/resource_management/resource_pool.h>
#include "nx_ec/data/api_conversion_functions.h"
#include <nx/utils/log/log.h>

namespace ec2
{

QnResourceNotificationManager::QnResourceNotificationManager() {}

void QnResourceNotificationManager::triggerNotification(
    const QnTransaction<ApiResourceStatusData>& tran, 
    NotificationSource source)
{
    NX_LOG(lit("%1 Emit statusChanged signal for resource %2")
        .arg(QString::fromLatin1(Q_FUNC_INFO))
        .arg(tran.params.id.toString()), cl_logDEBUG2);
    emit statusChanged(QnUuid(tran.params.id), tran.params.status, source);
}

void QnResourceNotificationManager::triggerNotification(
    const QnTransaction<ApiLicenseOverflowData>& /*tran*/, 
    NotificationSource /*source*/) 
{
    // nothing to do
}

void QnResourceNotificationManager::triggerNotification(const QnTransaction<ApiCleanupDatabaseData>& /*tran*/, NotificationSource /*source*/)
{
    // nothing to do
}

void QnResourceNotificationManager::triggerNotification(
    const QnTransaction<ApiResourceParamWithRefData>& tran, 
    NotificationSource /*source*/)
{
    if (tran.command == ApiCommand::setResourceParam)
        emit resourceParamChanged(tran.params);
    else if (tran.command == ApiCommand::removeResourceParam)
        emit resourceParamRemoved(tran.params);
}

void QnResourceNotificationManager::triggerNotification(
    const QnTransaction<ApiResourceParamWithRefDataList>& tran, 
    NotificationSource /*source*/) 
{
    for (const ec2::ApiResourceParamWithRefData& param : tran.params)
    {
        if (tran.command == ApiCommand::setResourceParams)
            emit resourceParamChanged(param);
        else if (tran.command == ApiCommand::removeResourceParams)
            emit resourceParamRemoved(param);
    }
}

void QnResourceNotificationManager::triggerNotification(
    const QnTransaction<ApiIdData>& tran, 
    NotificationSource /*source*/)
{
    if (tran.command == ApiCommand::removeResourceStatus)
        emit resourceStatusRemoved(tran.params.id);
    else
        emit resourceRemoved(tran.params.id);
}

void QnResourceNotificationManager::triggerNotification(
    const QnTransaction<ApiIdDataList>& tran, 
    NotificationSource /*source*/) 
{
    for (const ApiIdData& id : tran.params)
        emit resourceRemoved(id.id);
}

} // namespace ec2
