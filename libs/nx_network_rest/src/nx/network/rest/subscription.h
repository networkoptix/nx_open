// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <memory>
#include <set>
#include <unordered_map>

#include <QtCore/QString>

#include <nx/utils/async_operation_guard.h>
#include <nx/utils/move_only_func.h>
#include <nx/utils/scope_guard.h>

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
        m_subscriptions.emplace(std::move(subscription), std::move(request));
        return guard;
    }

    template<typename T>
    void notifyWithPayload(const QString& id, NotifyType notifyType, std::optional<nx::Uuid> user,
        nx::MoveOnlyFunc<json_rpc::Payload<T>(const nx::Uuid&)> getPayload,
        nx::MoveOnlyFunc<PostProcessContext(const Request&)> makePostProcess = {},
        const json::OpenApiSchemas* schemas = {}) const
    {
        if (makePostProcess)
        {
            auto callbacks = this->callbacks(id, std::move(user), std::move(makePostProcess));
            for (auto& [user, callbacks]: callbacks)
            {
                auto payload = getPayload(user);
                if (!payload.data)
                    continue;

                rapidjson::Document extensions;
                if (payload.extensions)
                    extensions = nx::json::serialized(*payload.extensions, /*stripDefault*/ false);
                for (auto& [callback, postProcess]: callbacks)
                {
                    auto data = *payload.data;
                    detail::filter(&data, postProcess.filters);
                    detail::orderBy(&data, postProcess.filters);
                    auto document = json::serialize(
                        data, std::move(postProcess.filters), postProcess.defaultValueAction);
                    if (schemas)
                        schemas->postprocessResponse(postProcess.method, postProcess.path, &document);
                    (*callback)(id, notifyType, &document, &extensions);
                }
            }
        }
        else
        {
            auto callbacks = this->callbacks(id, std::move(user));
            for (auto& [user, callbacks]: callbacks)
            {
                auto payload = getPayload(user);
                if (!payload.data)
                    continue;

                auto document = nx::json::serialized(*payload.data, /*stripDefault*/ false);
                rapidjson::Document extensions;
                if (payload.extensions)
                    extensions = nx::json::serialized(*payload.extensions, /*stripDefault*/ false);
                for (auto& callback: callbacks)
                    (*callback)(id, notifyType, &document, &extensions);
            }
        }
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

    static nx::network::rest::Request payloadRequest(
        const QString& subscriptionId, const nx::Uuid& userId)
    {
        using namespace nx::network::rest;
        return {json_rpc::Context{.subscribed = true, .subscriptionId = subscriptionId},
            {.session = AuthSession{nx::Uuid{}}, .access = UserAccessData{userId}}};
    }

private:
    using Callback = std::shared_ptr<SubscriptionCallback>;

    std::map<nx::Uuid, std::vector<std::pair<Callback, PostProcessContext>>> callbacks(
        const QString& id,
        std::optional<nx::Uuid> user,
        nx::MoveOnlyFunc<PostProcessContext(const Request&)> makePostProcess) const
    {
        std::map<nx::Uuid, std::vector<std::pair<Callback, PostProcessContext>>> result;
        auto fillResult =
            [this, &result, user = std::move(user), makePostProcess = std::move(makePostProcess)](
                const auto& callbacks)
            {
                for (const auto& callback: callbacks)
                {
                    const auto& request = m_subscriptions.at(callback);
                    const auto userId = request.userSession.access.userId;
                    if (!user || *user == userId)
                        result[userId].push_back({callback, makePostProcess(request)});
                }
            };
        auto lock(m_asyncOperationGuard->lock());
        if (auto it = m_subscribedIds.find(id); it != m_subscribedIds.end())
            fillResult(it->second);
        if (auto it = m_subscribedIds.find(QString("*")); it != m_subscribedIds.end())
            fillResult(it->second);
        return result;
    }

    std::map<nx::Uuid, std::vector<Callback>> callbacks(
        const QString& id, std::optional<nx::Uuid> user) const
    {
        std::map<nx::Uuid, std::vector<Callback>> result;
        auto fillResult =
            [this, &result, user = std::move(user)](const auto& callbacks)
            {
                for (const auto& callback: callbacks)
                {
                    const auto& request = m_subscriptions.at(callback);
                    const auto userId = request.userSession.access.userId;
                    if (!user || *user == userId)
                        result[userId].push_back(callback);
                }
            };
        auto lock(m_asyncOperationGuard->lock());
        if (auto it = m_subscribedIds.find(id); it != m_subscribedIds.end())
            fillResult(it->second);
        if (auto it = m_subscribedIds.find(QString("*")); it != m_subscribedIds.end())
            fillResult(it->second);
        return result;
    }

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
