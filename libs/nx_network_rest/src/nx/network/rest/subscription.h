// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <memory>
#include <set>
#include <unordered_map>

#include <QtCore/QString>

#include <nx/utils/async_operation_guard.h>
#include <nx/utils/move_only_func.h>
#include <nx/utils/scope_guard.h>

#include "api_versions.h"
#include "crud_handler.h"
#include "json_rpc/client_extensions.h"
#include "open_api_schema.h"

namespace nx::network::rest {

class SubscriptionHandler
{
public:
    using AddMonitor = nx::MoveOnlyFunc<nx::utils::Guard()>;
    using NotifyType = Handler::NotifyType;
    using SubscriptionCallback = Handler::SubscriptionCallback;

    struct PostProcessContext
    {
        Params filters;
        nx::network::http::Method method;
        QString path;
        json::DefaultValueAction defaultValueAction = json::DefaultValueAction::appendMissing;
    };

    SubscriptionHandler(AddMonitor addMonitor): m_addMonitor(std::move(addMonitor)) {}
    virtual ~SubscriptionHandler()
    {
        std::unordered_map<std::shared_ptr<SubscriptionCallback>, Request> subscriptions;
        std::unordered_map<QString, std::set<std::shared_ptr<SubscriptionCallback>>> subscribedIds;
        nx::utils::Guard guard;
        auto lock(m_asyncOperationGuard->lock());
        guard = std::move(m_guard);
        subscriptions = std::move(m_subscriptions);
        subscribedIds = std::move(m_subscribedIds);
    }

    nx::utils::Guard addSubscription(Request request, SubscriptionCallback callback)
    {
        const auto& id = request.jsonRpcContext()->subscriptionId;
        auto subscription = std::make_shared<SubscriptionCallback>(std::move(callback));
        NX_VERBOSE(this,
            "Add subscription %1 %2 for connection %3",
            subscription.get(), id, request.jsonRpcContext()->connection.id);

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
        m_subscriptions.emplace(std::move(subscription), std::move(request));
        return guard;
    }

    void notify(
        const QString& id,
        NotifyType notifyType,
        rapidjson::Document data,
        rapidjson::Document extensions) const
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
            (*callback)(id, notifyType, &data, &extensions);
        for (auto callback: holder2)
            (*callback)(id, notifyType, &data, &extensions);
    }

    void setAddMonitor(AddMonitor addMonitor)
    {
        NX_ASSERT(!m_guard);
        m_addMonitor = std::move(addMonitor);
    }

    static Request payloadRequest(
        const QString& subscriptionId,
        const nx::Uuid& userId,
        size_t apiVersion = kLatestApiVersion,
        json_rpc::WeakConnection connection = {})
    {
        using namespace nx::network::rest;
        auto userAccessData = UserAccessData{userId};
        if (userId == kCloudServiceUserAccess.userId || userId == kVideowallUserAccess.userId)
            userAccessData.access = UserAccessData::Access::ReadAllResources;
        Request r{
            json_rpc::Context{
                .connection = std::move(connection),
                .subscribed = true,
                .subscriptionId = subscriptionId},
            {.session = AuthSession{nx::Uuid{}}, .access = std::move(userAccessData)}};
        r.setApiVersion(apiVersion);
        return r;
    }

protected:
    using Callback = std::shared_ptr<SubscriptionCallback>;
    using Connection = json_rpc::WeakConnection;

    struct UpdateCallBack
    {
        size_t apiVersion = kLatestApiVersion;
        QString subscriptionId;
        Callback callback;
        PostProcessContext postProcess;
    };

    std::map<nx::Uuid, std::map<Connection, std::vector<UpdateCallBack>>>
        callbacks(
            const QString& id,
            std::optional<nx::Uuid> user,
            nx::MoveOnlyFunc<PostProcessContext(const Request&)> makePostProcess) const
    {
        std::map<nx::Uuid, std::map<Connection, std::vector<UpdateCallBack>>> result;
        auto fillResult =
            [this, &result, user = std::move(user), makePostProcess = std::move(makePostProcess)](
                const auto& id, const auto& callbacks)
            {
                for (const auto& callback: callbacks)
                {
                    const auto& request = m_subscriptions.at(callback);
                    const auto userId = request.userSession.access.userId;
                    if (!user || *user == userId)
                    {
                        result[userId][request.jsonRpcContext()->connection].push_back(
                            {*request.apiVersion(), id, callback, makePostProcess(request)});
                    }
                }
            };
        auto lock(m_asyncOperationGuard->lock());
        if (auto it = m_subscribedIds.find(id); it != m_subscribedIds.end())
            fillResult(it->first, it->second);
        if (auto it = m_subscribedIds.find(QString("*")); it != m_subscribedIds.end())
            fillResult(it->first, it->second);
        return result;
    }

    std::map<nx::Uuid, std::map<Connection, std::vector<std::tuple<QString, Callback>>>> callbacks(
        const QString& id, std::optional<nx::Uuid> user) const
    {
        std::map<nx::Uuid, std::map<Connection, std::vector<std::tuple<QString, Callback>>>> result;
        auto fillResult =
            [this, &result, user = std::move(user)](const auto& id, const auto& callbacks)
            {
                for (const auto& callback: callbacks)
                {
                    const auto& request = m_subscriptions.at(callback);
                    const auto userId = request.userSession.access.userId;
                    if (!user || *user == userId)
                        result[userId][request.jsonRpcContext()->connection].push_back({id, callback});
                }
            };
        auto lock(m_asyncOperationGuard->lock());
        if (auto it = m_subscribedIds.find(id); it != m_subscribedIds.end())
            fillResult(it->first, it->second);
        if (auto it = m_subscribedIds.find(QString("*")); it != m_subscribedIds.end())
            fillResult(it->first, it->second);
        return result;
    }

private:
    nx::utils::Guard removeSubscription(const std::shared_ptr<SubscriptionCallback>& subscription)
    {
        NX_VERBOSE(this, "Remove subscription %1", subscription.get());
        if (auto s = m_subscriptions.find(subscription); s != m_subscriptions.end())
        {
            if (auto it = m_subscribedIds.find(s->second.jsonRpcContext()->subscriptionId);
                it != m_subscribedIds.end())
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
    std::unordered_map<std::shared_ptr<SubscriptionCallback>, Request> m_subscriptions;
    std::unordered_map<QString, std::set<std::shared_ptr<SubscriptionCallback>>> m_subscribedIds;
    AddMonitor m_addMonitor;
};

} // namespace nx::network::rest
