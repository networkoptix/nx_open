// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "analytics_notification_manager.h"

namespace ec2 {

void AnalyticsNotificationManager::triggerNotification(
    const QnTransaction<nx::vms::api::AnalyticsPluginData>& tran,
    NotificationSource source)
{
    NX_ASSERT(tran.command == ApiCommand::saveAnalyticsPlugin);
    emit analyticsPluginAddedOrUpdated(tran.params, source);
}

void AnalyticsNotificationManager::triggerNotification(
    const QnTransaction<nx::vms::api::AnalyticsEngineData>& tran,
    NotificationSource source)
{
    NX_ASSERT(tran.command == ApiCommand::saveAnalyticsEngine);
    emit analyticsEngineAddedOrUpdated(tran.params, source);
}

void AnalyticsNotificationManager::triggerNotification(
    const QnTransaction<nx::vms::api::IdData>& tran,
    NotificationSource source)
{
    switch (tran.command)
    {
        case ApiCommand::removeAnalyticsPlugin:
            emit analyticsPluginRemoved(nx::Uuid(tran.params.id), source);
            break;
        case ApiCommand::removeAnalyticsEngine:
            emit analyticsEngineRemoved(nx::Uuid(tran.params.id), source);
            break;
        default:
            NX_ASSERT(false, "Wrong transaction type");
    }
}

} // namespace ec2
