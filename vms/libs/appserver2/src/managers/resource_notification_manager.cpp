// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

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
    const QnTransaction<vms::api::VideoWallLicenseOverflowData>& /*tran*/,
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
    NotificationSource source)
{
    if (tran.command == ApiCommand::setResourceParam)
        emit resourceParamChanged(tran.params, source);
    else if (tran.command == ApiCommand::removeResourceParam)
        emit resourceParamRemoved(tran.params);
}

void QnResourceNotificationManager::triggerNotification(
    const QnTransaction<vms::api::ResourceParamWithRefDataList>& tran,
    NotificationSource source)
{
    for (const auto& param: tran.params)
    {
        if (tran.command == ApiCommand::setResourceParams)
            emit resourceParamChanged(param, source);
        else if (tran.command == ApiCommand::removeResourceParams)
            emit resourceParamRemoved(param);
    }
}

void QnResourceNotificationManager::triggerNotification(
    const QnTransaction<vms::api::IdData>& tran,
    NotificationSource source)
{
    if (tran.command == ApiCommand::removeResourceStatus)
        emit resourceStatusRemoved(tran.params.id, source);
    else
        emit resourceRemoved(tran.params.id, source);
}

void QnResourceNotificationManager::triggerNotification(
    const QnTransaction<vms::api::IdDataList>& tran,
    NotificationSource source)
{
    for (const vms::api::IdData& id: tran.params)
        emit resourceRemoved(id.id, source);
}

} // namespace ec2
