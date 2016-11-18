#include "flir_nexus_string_builder.h"
#include "flir_nexus_common.h"

namespace nx {
namespace plugins {
namespace flir {
namespace nexus {

SubscriptionStringBuilder::SubscriptionStringBuilder():
    m_sessionId(-1),
    m_notificationFormat(kStringNotificationFormat)
{
}

QString SubscriptionStringBuilder::buildSubscriptionString(
    const std::vector<Subscription>& subscriptions) const
{
    QUrlQuery query;

    query.addQueryItem(kSessionParamName, QString::number(m_sessionId));
    query.addQueryItem(kSubscriptionNumParamName, QString::number(subscriptions.size()));
    query.addQueryItem(kNotificationFormatParamName, m_notificationFormat);

    for (auto i = 0; i < subscriptions.size(); ++i)
    {
        query.addQueryItem(
            lit("subscription%1").arg(i + 1),
            serializeSingleSubscription(subscriptions[i]));
    }

    return kSubscriptionPrefix + lit("?") + query.toString(QUrl::FullyEncoded);
}

void SubscriptionStringBuilder::setSessionId(int sessionId)
{
    m_sessionId = sessionId;
}

void SubscriptionStringBuilder::setNotificationFormat(const QString& notificationFormat)
{
    m_notificationFormat = notificationFormat;
}

QString SubscriptionStringBuilder::serializeSingleSubscription(
    const Subscription& subscription) const
{
    auto subscriptionString = lit("\"%1,%2,%3,%4,%5\"")
        .arg(subscription.subscriptionType)
        .arg(subscription.deviceId)
        .arg(subscription.minDeliveryInterval.count())
        .arg(subscription.maxDeliveryInterval.count())
        .arg(subscription.onChange);

    qDebug() << "Flir, building subscription string" << subscriptionString;

    return subscriptionString;
}

} // namespace nexus
} // namespace flir
} // namespace plugins
} // namespace nx


