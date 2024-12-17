// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <memory>
#include <set>
#include <unordered_map>

#include <QtCore/QString>

#include <nx/utils/move_only_func.h>
#include <nx/utils/scope_guard.h>
#include <nx/utils/thread/mutex.h>

#include "handler.h"

namespace nx::network::rest {

template<typename Data>
class SubscriptionHandler
{
public:
    using AddMonitor = nx::utils::MoveOnlyFunc<nx::utils::Guard()>;
    using NotifyType = Handler::NotifyType;
    using SubscriptionCallback =
        nx::utils::MoveOnlyFunc<void(const QString& id, NotifyType, const Data& data)>;

    SubscriptionHandler(AddMonitor addMonitor): m_addMonitor(std::move(addMonitor)) {}
    virtual ~SubscriptionHandler() = default;

    nx::utils::Guard addSubscription(const QString& id, const Request&, SubscriptionCallback callback)
    {
        auto subscription = std::make_shared<SubscriptionCallback>(std::move(callback));
        NX_VERBOSE(this, "Add subscription %1 for %2", subscription.get(), id);

        nx::utils::Guard guard{[this, subscription]() { removeSubscription(subscription); }};
        NX_MUTEX_LOCKER lock(&m_mutex);
        if (m_subscribedIds.empty())
            m_guard = m_addMonitor();
        m_subscribedIds[id].emplace(subscription);
        m_subscriptions.emplace(std::move(subscription), id);
        return guard;
    }

    void notify(const QString& id, NotifyType notifyType, const Data& data)
    {
        std::set<std::shared_ptr<SubscriptionCallback>> holder1;
        std::set<std::shared_ptr<SubscriptionCallback>> holder2;
        {
            NX_MUTEX_LOCKER lock(&m_mutex);
            if (auto it = m_subscribedIds.find(id); it != m_subscribedIds.end())
                holder1 = it->second;
            if (auto it = m_subscribedIds.find(QString("*")); it != m_subscribedIds.end())
                holder2 = it->second;
        }
        for (auto callback: holder1)
            (*callback)(id, notifyType, data);
        for (auto callback: holder2)
            (*callback)(id, notifyType, data);
    }

    void setAddMonitor(AddMonitor addMonitor)
    {
        NX_ASSERT(!m_guard);
        m_addMonitor = std::move(addMonitor);
    }

private:
    void removeSubscription(const std::shared_ptr<SubscriptionCallback>& subscription)
    {
        NX_VERBOSE(this, "Remove subscription %1", subscription.get());
        nx::utils::Guard guard;
        {
            NX_MUTEX_LOCKER lock(&m_mutex);
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
                guard = std::move(m_guard);
        }
    }

private:
    nx::Mutex m_mutex;
    nx::utils::Guard m_guard;
    std::unordered_map<std::shared_ptr<SubscriptionCallback>, QString> m_subscriptions;
    std::unordered_map<QString, std::set<std::shared_ptr<SubscriptionCallback>>> m_subscribedIds;
    AddMonitor m_addMonitor;
};

} // namespace nx::network::rest
