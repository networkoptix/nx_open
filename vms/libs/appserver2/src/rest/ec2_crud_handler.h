// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <tuple>

#include <core/resource/resource_type.h>
#include <database/db_manager.h>
#include <nx/network/rest/params.h>
#include <nx/network/rest/request.h>
#include <nx/vms/ec2/ec_connection_notification_manager.h>
#include <transaction/transaction.h>

#include "details.h"

namespace ec2 {

template<
    typename Filter,
    typename Model,
    typename DeleteInput,
    typename QueryProcessor,
    ApiCommand::Value DeleteCommand
>
class CrudHandler
{
public:
    CrudHandler(QueryProcessor* queryProcessor): m_queryProcessor(queryProcessor) {}

    std::vector<Model> read(Filter filter, const nx::network::rest::Request& request)
    {
        using namespace details;

        auto processor = m_queryProcessor->getAccess(request.userSession);
        validateType(processor, filter, DeleteCommand);

        auto query =
            [id = std::move(filter.getId()), &processor](auto&& x)
            {
                using Arg = std::decay_t<decltype(x)>;
                using Output = std::conditional_t<IsVector<Arg>::value, Arg, std::vector<Arg>>;
                auto future = processor.template processQueryAsync<decltype(id), Output>(
                    getReadCommand<Output>(),
                    std::move(id),
                    std::make_pair<Result, Output>);
                future.waitForFinished();
                auto [result, output] = future.resultAt(0);
                if (!result)
                    throwError(std::move(result));
                return output;
            };
        if constexpr (fromDbTypesExists<Model>::value)
        {
            return Model::fromDbTypes(
                callQueryFunc(typename Model::DbReadTypes(), std::move(query)));
        }
        else
        {
            return query(Model());
        }
    }

    void delete_(DeleteInput id, const nx::network::rest::Request& request)
    {
        using namespace details;

        auto processor = m_queryProcessor->getAccess(request.userSession);
        validateType(processor, id, DeleteCommand);

        std::promise<Result> promise;
        processor.processUpdateAsync(
            DeleteCommand,
            std::move(id),
            [&promise](Result result) { promise.set_value(std::move(result)); });
        if (Result result = promise.get_future().get(); !result)
            throwError(std::move(result));
    }

    void create(Model data, const nx::network::rest::Request& request)
    {
        update(std::move(data), request);
    }

    void update(Model data, const nx::network::rest::Request& request)
    {
        using namespace details;

        std::promise<Result> promise;
        auto processor = m_queryProcessor->getAccess(request.userSession);
        if constexpr (toDbTypesExists<Model>::value)
        {
            auto id = data.getId();
            auto items = std::move(data).toDbTypes();
            using IgnoredDbType = std::decay_t<decltype(std::get<0>(items))>;
            callUpdateFunc(items,
                [&processor](const auto& x)
                {
                    if (const auto r = validateType(processor, x, DeleteCommand); !r)
                        return r;
                    return validateResourceTypeId(x);
                });

            auto update =
                [id = std::move(id)](auto&& x, auto& copy, auto* list)
                {
                    using DbType = std::decay_t<decltype(x)>;
                    Result result;
                    assertModelToDbTypesProducedValidResult<IgnoredDbType, DbType, Model>(x, id);
                    if constexpr (IsVector<DbType>::value)
                    {
                        const auto command = getUpdateCommand<std::decay_t<decltype(x[0])>>();
                        result = copy.template processMultiUpdateSync<DbType>(
                            parentUpdateCommand(command),
                            command,
                            TransactionType::Regular,
                            std::move(x),
                            list);
                    }
                    else
                    {
                        result =
                            copy.processUpdateSync(getUpdateCommand<DbType>(), std::move(x), list);
                    }
                    return result;
                };
            processor.processCustomUpdateAsync(
                ApiCommand::CompositeSave,
                [&promise](Result result) { promise.set_value(std::move(result)); },
                [update = std::move(update), items = std::move(items)](auto& copy, auto* list)
                {
                    return callUpdateFunc(
                        std::move(items),
                        [update = std::move(update), &copy, list](auto&& x) mutable
                        {
                            if constexpr (IsOptional<std::decay_t<decltype(x)>>::value)
                                return x ? update(std::move(*x), copy, list) : Result();
                            else
                                return update(std::move(x), copy, list);
                        });
                });
        }
        else
        {
            validateType(processor, data, DeleteCommand);
            processor.processUpdateAsync(
                getUpdateCommand<Model>(),
                data,
                [&promise](Result result) { promise.set_value(std::move(result)); });
        }
        if (Result result = promise.get_future().get(); !result)
            throwError(std::move(result));
    }

    using SubscriptionCallback = nx::utils::MoveOnlyFunc<void()>;
    nx::utils::Guard doSubscribe(const QString& id, SubscriptionCallback callback)
    {
        using namespace details;

        NX_MUTEX_LOCKER lock(&m_mutex);
        if (m_subscribedIds.empty())
        {
            std::vector<ApiCommand::Value> commands{{DeleteCommand}};
            invokeOnDbTypes<applyComma, Model>(
                [&commands](auto&& data)
                {
                    commands.push_back(getUpdateCommand<std::decay_t<decltype(data)>>());
                });
            m_guard = m_queryProcessor->addMonitor(
                [this](const auto& transaction)
                {
                    invokeOnDbTypes<applyOr, Model>(
                        [this, &transaction](auto&& data)
                        {
                            const auto doNotify =
                                [this](const auto& transaction)
                                {
                                    const auto it =
                                        m_subscribedIds.find(nx::toString(transaction.params.getId()));
                                    if (it == m_subscribedIds.end())
                                        return false;

                                    for (auto callback: it->second)
                                        (*callback)();
                                    return true;
                                };

                            NX_MUTEX_LOCKER lock(&m_mutex);
                            if (DeleteCommand == transaction.command)
                                return doNotify(static_cast<const QnTransaction<DeleteInput>&>(transaction));

                            using T = std::decay_t<decltype(data)>;
                            // TODO: Remove `details::` when MSVC v17 will be used.
                            if (details::getUpdateCommand<T>() == transaction.command)
                                return doNotify(static_cast<const QnTransaction<T>&>(transaction));

                            return false;
                        });
                },
                std::move(commands));
        }

        auto subscription = std::make_shared<SubscriptionCallback>(std::move(callback));
        nx::utils::MoveOnlyFunc<void()> guard =
            [this, subscription]()
            {
                NX_MUTEX_LOCKER lock(&m_mutex);
                if (auto s = m_subscriptions.find(subscription); s != m_subscriptions.end())
                {
                    if (auto it = m_subscribedIds.find(s->second); it != m_subscribedIds.end())
                    {
                        it->second.erase(subscription.get());
                        if (it->second.empty())
                            m_subscribedIds.erase(it);
                    }
                    m_subscriptions.erase(s);
                }
                if (m_subscribedIds.empty())
                    m_guard.fire();
            };
        m_subscribedIds[id].emplace(subscription.get());
        m_subscriptions.emplace(std::move(subscription), id);
        return guard;
    }

    QString getSubscriptionId(const nx::network::rest::Request& request)
    {
        if (request.method() == nx::network::http::Method::get)
        {
            return request.params().toJson(/*excludeCommon*/ true).isEmpty()
                ? QString("*") //< TODO: Support subscription for all ids.
                : nx::toString(request.parseContentOrThrow<Filter>().getId());
        }

        if (request.method() == nx::network::http::Method::delete_)
            return nx::toString(request.parseContentOrThrow<DeleteInput>().getId());

        NX_ASSERT(false, "Use `get` to `subscribe` or `delete` to `unsubscribe`");
        return {};
    }

protected:
    QueryProcessor* const m_queryProcessor;

private:
    nx::Mutex m_mutex;
    nx::utils::Guard m_guard;
    std::unordered_map<std::shared_ptr<SubscriptionCallback>, QString> m_subscriptions;
    std::unordered_map<QString, std::set<SubscriptionCallback*>> m_subscribedIds;
};

} // namespace ec2
