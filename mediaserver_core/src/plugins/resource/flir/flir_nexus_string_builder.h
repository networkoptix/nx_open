#pragma once

#include "flir_nexus_common.h"

#include <vector>
#include <chrono>

namespace nx{
namespace plugins{
namespace flir{
namespace nexus{

struct Subscription final
{
    Subscription(const QString& subscriptionTypeString):
        subscriptionType(subscriptionTypeString)
    {};

    QString subscriptionType;
    int deviceId = kAnyDevice;
    std::chrono::milliseconds minDeliveryInterval = std::chrono::milliseconds(1000);
    std::chrono::milliseconds maxDeliveryInterval = std::chrono::milliseconds(1000);
    bool onChange = false;
};

class SubscriptionStringBuilder final
{
public:
    SubscriptionStringBuilder();
    QString buildSubscriptionString(const std::vector<Subscription>& subscriptions) const;

    void setSessionId(int sessionId);
    void setNotificationFormat(const QString& notificationFormat);

private:
    QString serializeSingleSubscription(const Subscription& subscription) const;

private:
    int m_sessionId;
    QString m_notificationFormat;
};

} // namespace nexus
} // namespace flir
} // namespace plugins
} // namespace nx