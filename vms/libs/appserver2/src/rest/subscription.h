// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <memory>
#include <set>
#include <unordered_map>

#include <QtCore/QJsonValue>
#include <QtCore/QString>

#include <nx/utils/async_operation_guard.h>
#include <nx/utils/move_only_func.h>
#include <nx/utils/scope_guard.h>

namespace ec2 {

enum class NotifyType
{
    update,
    delete_
};

using SubscriptionCallback =
    nx::utils::MoveOnlyFunc<void(const QString& id, NotifyType, QJsonValue payload)>;

class SubscriptionHandler
{
public:
    using AddMonitor = nx::utils::MoveOnlyFunc<nx::utils::Guard()>;
    SubscriptionHandler(AddMonitor addMonitor): m_addMonitor(std::move(addMonitor)) {}
    virtual ~SubscriptionHandler();
    nx::utils::Guard addSubscription(const QString& id, SubscriptionCallback callback);
    void notify(const QString& id, NotifyType notifyType, QJsonValue payload = {});

    void setAddMonitor(AddMonitor addMonitor)
    {
        NX_ASSERT(!m_guard);
        m_addMonitor = std::move(addMonitor);
    }

private:
    nx::utils::Guard removeSubscription(const std::shared_ptr<SubscriptionCallback>& subscription);

private:
    nx::utils::AsyncOperationGuard m_asyncOperationGuard;
    nx::utils::Guard m_guard;
    std::unordered_map<std::shared_ptr<SubscriptionCallback>, QString> m_subscriptions;
    std::unordered_map<QString, std::set<std::shared_ptr<SubscriptionCallback>>> m_subscribedIds;
    AddMonitor m_addMonitor;
};

} // namespace ec2
