#pragma once

#include "flir_nexus_common.h"

#include <vector>
#include <chrono>

namespace nx{
namespace plugins{
namespace flir{
namespace nexus{

class SubscriptionStringBuilder final
{
public:
    SubscriptionStringBuilder();
    QString buildSubscriptionString(const std::vector<Subscription>& subscriptions) const;

    void setSessionId(int sessionId);
    void setNotificationFormat(const QString& notificationFormat);

private:
    int m_sessionId;
    QString m_notificationFormat;
};

} // namespace nexus
} // namespace flir
} // namespace plugins
} // namespace nx