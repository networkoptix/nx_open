// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <memory>
#include <set>
#include <unordered_map>

#include <QtCore/QString>

#include <nx/utils/async_operation_guard.h>
#include <nx/utils/move_only_func.h>
#include <nx/utils/scope_guard.h>

#include "handler.h"

namespace nx::network::rest {

class SubscriptionHandler
{
public:
    using AddMonitor = nx::MoveOnlyFunc<nx::utils::Guard()>;
    using NotifyType = Handler::NotifyType;
    using SubscriptionCallback = Handler::SubscriptionCallback;

    SubscriptionHandler(AddMonitor addMonitor): m_addMonitor(std::move(addMonitor)) {}
    virtual ~SubscriptionHandler()
    {
        std::unordered_map<std::shared_ptr<SubscriptionCallback>, QString> subscriptions;
        std::unordered_map<QString, std::set<std::shared_ptr<SubscriptionCallback>>> subscribedIds;
        nx::utils::Guard guard;
        auto lock(m_asyncOperationGuard->lock());
        guard = std::move(m_guard);
        subscriptions = std::move(m_subscriptions);
        subscribedIds = std::move(m_subscribedIds);
    }

    nx::utils::Guard addSubscription(const Request& request, SubscriptionCallback callback)
    {
        const auto& id = request.jsonRpcContext()->subscriptionId;
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

    void notify(
        const QString& id,
        NotifyType notifyType,
        rapidjson::Document data,
        std::optional<nx::Uuid> user = {},
        bool noEtag = false) const
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
            (*callback)(id, notifyType, &data, std::move(user), noEtag);
        for (auto callback: holder2)
            (*callback)(id, notifyType, &data, std::move(user), noEtag);
    }

    void setAddMonitor(AddMonitor addMonitor)
    {
        NX_ASSERT(!m_guard);
        m_addMonitor = std::move(addMonitor);
    }

private:
    nx::utils::Guard removeSubscription(const std::shared_ptr<SubscriptionCallback>& subscription)
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

private:
    nx::utils::AsyncOperationGuard m_asyncOperationGuard;
    nx::utils::Guard m_guard;
    std::unordered_map<std::shared_ptr<SubscriptionCallback>, QString> m_subscriptions;
    std::unordered_map<QString, std::set<std::shared_ptr<SubscriptionCallback>>> m_subscribedIds;
    AddMonitor m_addMonitor;
};

} // namespace nx::network::rest
