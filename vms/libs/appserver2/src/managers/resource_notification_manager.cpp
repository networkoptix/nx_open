#include "resource_notification_manager.h"

#include <core/resource_management/resource_pool.h>
#include <nx_ec/data/api_conversion_functions.h>

#include <nx/utils/log/log.h>

using namespace nx;

namespace ec2 {

QnResourceNotificationManager::QnResourceNotificationManager()
{
}

void QnResourceNotificationManager::triggerNotification(
    const QnTransaction<vms::api::ResourceStatusData>& tran,
    NotificationSource source)
{
    NX_VERBOSE(this, lit("%1 Emit statusChanged signal for resource %2")
        .arg(QString::fromLatin1(Q_FUNC_INFO))
        .arg(tran.params.id.toString()));
    emit statusChanged(QnUuid(tran.params.id), tran.params.status, source);
}

void QnResourceNotificationManager::triggerNotification(
    const QnTransaction<vms::api::LicenseOverflowData>& /*tran*/,
    NotificationSource /*source*/)
{
}

void QnResourceNotificationManager::triggerNotification(
    const QnTransaction<vms::api::CleanupDatabaseData>& /*tran*/,
    NotificationSource /*source*/)
{
}

void QnResourceNotificationManager::triggerNotification(
    const QnTransaction<vms::api::ResourceParamWithRefData>& tran,
    NotificationSource /*source*/)
{
    if (tran.command == ApiCommand::setResourceParam)
        emit resourceParamChanged(tran.params);
    else if (tran.command == ApiCommand::removeResourceParam)
        emit resourceParamRemoved(tran.params);
}

void QnResourceNotificationManager::triggerNotification(
    const QnTransaction<vms::api::ResourceParamWithRefDataList>& tran,
    NotificationSource /*source*/)
{
    for (const auto& param: tran.params)
    {
        if (tran.command == ApiCommand::setResourceParams)
            emit resourceParamChanged(param);
        else if (tran.command == ApiCommand::removeResourceParams)
            emit resourceParamRemoved(param);
    }
}

void QnResourceNotificationManager::triggerNotification(
    const QnTransaction<vms::api::IdData>& tran,
    NotificationSource /*source*/)
{
    if (tran.command == ApiCommand::removeResourceStatus)
        emit resourceStatusRemoved(tran.params.id);
    else
        emit resourceRemoved(tran.params.id);
}

void QnResourceNotificationManager::triggerNotification(
    const QnTransaction<vms::api::IdDataList>& tran,
    NotificationSource /*source*/)
{
    for (const vms::api::IdData& id: tran.params)
        emit resourceRemoved(id.id);
}

} // namespace ec2
