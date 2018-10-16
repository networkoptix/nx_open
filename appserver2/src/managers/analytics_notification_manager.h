#pragma once

#include <transaction/transaction.h>
#include <nx_ec/managers/abstract_analytics_manager.h>

namespace ec2 {

class AnalyticsNotificationManager: public AbstractAnalyticsNotificationManager
{
public:
    AnalyticsNotificationManager() = default;

    void triggerNotification(
        const QnTransaction<nx::vms::api::AnalyticsPluginData>& tran,
        NotificationSource source);

    void triggerNotification(
        const QnTransaction<nx::vms::api::AnalyticsEngineData>& tran,
        NotificationSource source);

    void triggerNotification(
        const QnTransaction<nx::vms::api::IdData>& tran,
        NotificationSource source);
};

using AnalyticsNotificationManagerPtr = std::shared_ptr<AnalyticsNotificationManager>;

} // namespace ec2
