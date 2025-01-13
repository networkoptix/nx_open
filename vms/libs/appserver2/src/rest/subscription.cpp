// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "subscription.h"

#include <nx/utils/log/log_main.h>

namespace ec2 {

SubscriptionHandler::~SubscriptionHandler()
{
    std::unordered_map<std::shared_ptr<SubscriptionCallback>, QString> subscriptions;
    std::unordered_map<QString, std::set<std::shared_ptr<SubscriptionCallback>>> subscribedIds;
    nx::utils::Guard guard;
    auto lock(m_asyncOperationGuard->lock());
    guard = std::move(m_guard);
    subscriptions = std::move(m_subscriptions);
    subscribedIds = std::move(m_subscribedIds);
}

nx::utils::Guard SubscriptionHandler::addSubscription(
    const QString& id, SubscriptionCallback callback)
{
    auto subscription = std::make_shared<SubscriptionCallback>(std::move(callback));
    NX_VERBOSE(this, "Add subscription %1 for %2", subscription.get(), id);

    nx::utils::Guard guard{
        [this, subscription, sharedGuard = m_asyncOperationGuard.sharedGuard()]()
        {
            nx::utils::Guard guard;
            if (auto lock = sharedGuard->lock())
                guard = removeSubscription(subscription);
        }};
    auto lock(m_asyncOperationGuard->lock());
    if (m_subscribedIds.empty())
        m_guard = m_addMonitor();
    m_subscribedIds[id].emplace(subscription);
    m_subscriptions.emplace(std::move(subscription), id);
    return guard;
}

void SubscriptionHandler::notify(const QString& id, NotifyType notifyType, QJsonValue payload)
{
    std::set<std::shared_ptr<SubscriptionCallback>> holder1;
    std::set<std::shared_ptr<SubscriptionCallback>> holder2;
    {
        auto lock(m_asyncOperationGuard->lock());
        if (auto it = m_subscribedIds.find(id); it != m_subscribedIds.end())
            holder1 = it->second;
        if (auto it = m_subscribedIds.find(QString("*")); it != m_subscribedIds.end())
            holder2 = it->second;
    }
    for (auto callback: holder1)
        (*callback)(id, notifyType, payload);
    for (auto callback: holder2)
        (*callback)(id, notifyType, payload);
}

nx::utils::Guard SubscriptionHandler::removeSubscription(
    const std::shared_ptr<SubscriptionCallback>& subscription)
{
    NX_VERBOSE(this, "Remove subscription %1", subscription.get());
    if (auto s = m_subscriptions.find(subscription); s != m_subscriptions.end())
    {
        if (auto it = m_subscribedIds.find(s->second); it != m_subscribedIds.end())
        {
            it->second.erase(subscription);
            if (it->second.empty())
                m_subscribedIds.erase(it);
        }
        m_subscriptions.erase(s);
    }
    if (m_subscribedIds.empty())
        return std::move(m_guard);
    return {};
}

} // namespace ec2
