// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/network/rest/collection_hash.h>
#include <nx/network/rest/crud_handler.h>
#include <nx/network/rest/subscription.h>
#include <nx/utils/crud_model.h>
#include <nx/utils/data_structures/time_out_cache.h>
#include <nx/utils/elapsed_timer.h>
#include <nx/utils/i18n/scoped_locale.h>
#include <nx/utils/lockable.h>
#include <nx/utils/scope_guard.h>
#include <nx/utils/std_string_utils.h>
#include <transaction/fix_transaction_input_from_api.h>
#include <transaction/transaction_descriptor.h>

#include "details.h"

namespace ec2 {

template<
    typename Derived,
    typename Filter,
    typename Model,
    typename DeleteInput,
    typename QueryProcessor,
    ApiCommand::Value DeleteCommand
>
class CrudHandler:
    public nx::network::rest::CrudHandler<Derived>,
    public nx::network::rest::SubscriptionHandler
{
public:
    using base_type = nx::network::rest::CrudHandler<Derived>;
    using Request = nx::network::rest::Request;
    using ResponseAttributes = nx::network::rest::ResponseAttributes;

    template<typename... Args>
    CrudHandler(QueryProcessor* queryProcessor, Args&&... args):
        base_type(std::forward<Args>(args)...),
        SubscriptionHandler(
            [this]()
            {
                auto commands = details::commands<Model>(DeleteCommand);
                NX_VERBOSE(this, "Add monitor for %1", commands);
                auto guard = m_queryProcessor->addMonitor(
                    [this](const auto& transaction) { notify(transaction); }, std::move(commands));
                return nx::utils::Guard{
                    [this, guard = std::move(guard)]() { NX_VERBOSE(this, "Remove monitor"); }};
            }),
        m_queryProcessor(queryProcessor)
    {
    }

    [[nodiscard]]
    nx::i18n::ScopedLocale installScopedLocale(const nx::network::rest::Request& request)
    {
        if (auto manager = this->m_translationManager)
            return nx::i18n::ScopedLocale::install(manager, request.preferredResponseLocales());
        return {};
    }

    std::vector<Model> read(
        Filter filter, const Request& request, ResponseAttributes* responseAttributes)
    {
        using namespace details;

        auto processor = m_queryProcessor->getAccess(
            static_cast<const Derived*>(this)->prepareAuditRecord(request));
        auto id = nx::utils::model::getId(filter);
        validateType(processor, id, m_objectType);

        // `std::type_identity` (or similar) can be used to pass the read type info and avoid
        // useless `readType` object instantiation.
        const auto query =
            [this, &request, id, &processor](const auto& readType)
            {
                using Arg = std::decay_t<decltype(readType)>;
                using Output = std::conditional_t<nx::utils::Is<std::vector, Arg>(), Arg, std::vector<Arg>>;
                return processor.template processQueryAsync<decltype(id), Output>(
                    getReadCommand<Output>(),
                    std::move(id),
                    std::make_pair<Result, Output>,
                    [this, &request]() { return installScopedLocale(request); });
            };

        const auto readFuture =
            [](auto future)
            {
                future.waitForFinished();
                auto [result, output] = future.resultAt(0);
                if (!result)
                    throwError(std::move(result));
                return output;
            };

        std::vector<Model> result;
        if constexpr (fromDbTypesExists<Model>::value)
        {
            const auto logGuard = logTime("Query and Convert from DB types");

            auto futures = std::apply(
                [&](const auto&... readTypes)
                {
                    return std::make_tuple(query(readTypes)...);
                },
                typename Model::DbReadTypes());
            result = Model::fromDbTypes(
                std::apply(
                    [readFuture](auto&&... futures) -> typename Model::DbListTypes
                    {
                        return {readFuture(std::forward<decltype(futures)>(futures))...};
                    },
                    std::move(futures)));
        }
        else
        {
            result = readFuture(query(Model()));
        }
        return updateCombinedEtag(std::move(id), std::move(result), request, responseAttributes);
    }

    void delete_(DeleteInput id, const Request& request, ResponseAttributes* responseAttributes)
    {
        using namespace details;
        using namespace nx::utils::model;

        auto d = static_cast<Derived*>(this);
        nx::utils::Guard guard;
        if (responseAttributes)
        {
            auto lock = m_etags.lock();
            if (auto etags = lock->getValue(request.userSession.access.userId))
            {
                nx::network::http::insertOrReplaceHeader(&responseAttributes->httpHeaders,
                    {"ETag", nx::utils::toHex(etags->get().remove(d->subscriptionId(getId(id))))});
            }
            else
            {
                // CollectionHash for all items.
                guard = nx::utils::Guard([&]() { d->read({}, request, responseAttributes); });
            }
        }

        if (request.jsonRpcContext() && request.jsonRpcContext()->subscribed)
            return; //< Do nothing as this is a notification for a WebSocket connection.

        auto processor = m_queryProcessor->getAccess(d->prepareAuditRecord(request));
        validateType(processor, getId(id), m_objectType);

        std::promise<Result> promise;
        processor.processCustomUpdateAsync(
            ApiCommand::CompositeSave,
            [&promise](Result result) { promise.set_value(std::move(result)); },
            [this, &request, id = std::move(id)](auto& p, auto* list) mutable
            {
                auto scopedLocale = installScopedLocale(request);
                return p.processUpdateSync(DeleteCommand, std::move(id), list);
            });
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
        if constexpr (toDbTypesExists<Model>::value)
        {
            updateDbTypes(std::move(data).toDbTypes(), request);
        }
        else
        {
            std::promise<Result> promise;
            auto processor = m_queryProcessor->getAccess(this->prepareAuditRecord(request));
            validateType(processor, nx::utils::model::getId(data), m_objectType);
            validateResourceTypeId(data);
            processor.processCustomUpdateAsync(
                ApiCommand::CompositeSave,
                [&promise](Result result) { promise.set_value(std::move(result)); },
                [this, &request, data = std::move(data)](auto& p, auto* list) mutable
                {
                    auto scopedLocale = installScopedLocale(request);
                    return p.processUpdateSync(getUpdateCommand<Model>(), std::move(data), list);
                });
            if (Result result = promise.get_future().get(); !result)
                throwError(std::move(result));
        }
    }

    QString getSubscriptionId(const nx::network::rest::Request& request)
    {
        using namespace nx::network::rest;

        const auto it = request.param(this->m_idParamName);
        if (!it || *it == "*")
            return QString("*");

        if (it->isEmpty())
            throw Exception::invalidParameter(this->m_idParamName, *it);

        if (const auto& c = request.jsonRpcContext(); c && c->crud == json_rpc::Crud::all)
            return QString("*");

        auto d = static_cast<Derived*>(this);
        if (nx::Uuid::isUuidString(*it))
        {
            const auto id = nx::Uuid::fromStringSafe(*it);
            return id.isNull() ? QString("*") : d->subscriptionId(id);
        }

        if constexpr (requires { d->flexibleIdToId(*it); })
        {
            if (const auto id = d->flexibleIdToId(*it); id.isNull())
                throw Exception::notFound(NX_FMT("Resource '%1' is not found", *it));
            else
                return d->subscriptionId(id);
        }
        return d->subscriptionId(*it);
    }

    template<typename T>
    void checkSavePermission(const nx::network::rest::UserAccessData& access, const T& data) const
    {
        using namespace details;
        auto copy = data;
        if (auto r = fixRequestDataIfNeeded(&copy); !r)
            throwError(std::move(r));
        using MainDbType = std::decay_t<decltype(std::get<0>(typename Model::DbReadTypes{}))>;
        auto r = getActualTransactionDescriptorByValue<MainDbType>(getUpdateCommand<MainDbType>())
            ->checkSavePermissionFunc(m_queryProcessor->systemContext(), access, copy);
        if (!r)
            throwError(std::move(r));
    }

    static bool checkEtag(bool useId, const std::string& etagIn, const std::string& etagOut)
    {
        using Checker = nx::network::rest::CollectionHash;
        return Checker::check(useId ? Checker::item : Checker::list,
            nx::utils::fromHex(etagIn), nx::utils::fromHex(etagOut));
    }

protected:
    template<typename Id, typename Data>
    std::vector<Data> updateCombinedEtag(Id id, std::vector<Data> result,
        const Request& request, ResponseAttributes* responseAttributes)
    {
        if (!responseAttributes)
            return result;

        auto d = static_cast<Derived*>(this);
        if (id == Id{})
        {
            std::vector<nx::network::rest::CollectionHash::Item> data;
            data.reserve(result.size());
            for (const auto& item: result)
            {
                data.emplace_back(d->subscriptionId(nx::utils::model::getId(item)),
                    nx::reflect::json::serialize(item));
            }
            nx::network::rest::CollectionHash etags;
            nx::network::http::insertOrReplaceHeader(&responseAttributes->httpHeaders,
                {"ETag", nx::utils::toHex(etags.calculate(std::move(data)))});
            m_etags.lock()->put(request.userSession.access.userId, std::move(etags));
        }
        else if (!result.empty())
        {
            nx::network::rest::CollectionHash::Item item{
                d->subscriptionId(id), nx::reflect::json::serialize(result.front())};
            auto lock = m_etags.lock();
            auto etags = lock->getValue(request.userSession.access.userId);
            if (etags)
            {
                nx::network::http::insertOrReplaceHeader(&responseAttributes->httpHeaders,
                    {"ETag", nx::utils::toHex(etags->get().calculate(std::move(item)))});
            }
            else
            {
                lock.unlock();
                d->read({}, request, responseAttributes); //< CollectionHash for all items.
                if constexpr (requires { result = d->read(id, request, responseAttributes); })
                    result = d->read(std::move(id), request, responseAttributes);
            }
        }
        return result;
    }

    template<typename T>
    void updateDbTypes(T items, const nx::network::rest::Request& request)
    {
        using namespace details;
        auto processor = m_queryProcessor->getAccess(this->prepareAuditRecord(request));
        const auto& mainDbItem = std::get<0>(items);
        auto id = nx::utils::model::getId(mainDbItem);
        validateType(processor, id, m_objectType);
        validateResourceTypeId(mainDbItem);
        static_cast<Derived*>(this)->checkSavePermission(request.userSession.access, mainDbItem);
        using IgnoredDbType = std::decay_t<decltype(mainDbItem)>;
        auto update =
            [id](auto&& x, auto& copy, auto* list)
            {
                using DbType = std::decay_t<decltype(x)>;
                Result result;
                assertModelToDbTypesProducedValidResult<IgnoredDbType, DbType, Model>(x, id);
                if constexpr (nx::utils::Is<std::vector, DbType>())
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
        std::promise<Result> promise;
        processor.processCustomUpdateAsync(
            ApiCommand::CompositeSave,
            [&promise](Result result) { promise.set_value(std::move(result)); },
            [this, &request, update = std::move(update), items = std::move(items)](
                auto& copy, auto* list) mutable
            {
                auto scopedLocale = installScopedLocale(request);
                return applyFunc(
                    std::move(items),
                    [update = std::move(update), &copy, list](auto&& item)
                    {
                        if constexpr (nx::utils::Is<std::optional, std::decay_t<decltype(item)>>())
                            return item ? update(std::move(*item), copy, list) : Result();
                        else
                            return update(std::move(item), copy, list);
                    });
            });
        if (Result result = promise.get_future().get(); !result)
            throwError(std::move(result));
    }

    bool isValidType(const nx::Uuid& id) const
    {
        if constexpr (m_objectType == ApiObject_NotDefined)
            return true;

        if (id.isNull())
            return true;

        const auto objectType =
            m_queryProcessor->getAccess(nx::network::rest::kSystemSession).getObjectType(id);
        return objectType == ApiObject_NotDefined || objectType == m_objectType;
    }

    static QString subscriptionId(QString id) { return id; }
    static QString subscriptionId(const nx::Uuid& id) { return id.toSimpleString(); }

    void notify(const QString& id, NotifyType notifyType)
    {
        SubscriptionHandler::notify(id, notifyType, /*data*/ {});
    }

protected:
    QueryProcessor* const m_queryProcessor;

private:
    template<typename T>
    void validateType(const auto& processor, const T& id, ApiObjectType requiredType)
    {
        if (!static_cast<Derived*>(this)->isValidType(id))
        {
            throw nx::network::rest::Exception::notFound(
                NX_FMT("Object type %1 is not found", nx::reflect::json::serialize(id)));
        }
    }

    void notify(const QnAbstractTransaction& transaction)
    {
        using namespace details;
        using nx::utils::model::getId;

        if (DeleteCommand == transaction.command)
        {
            auto d = static_cast<Derived*>(this);
            const auto id =
                getId(static_cast<const QnTransaction<DeleteInput>&>(transaction).params);

            if (d->isValidType(id))
            {
                NX_VERBOSE(this, "Notify %1 for %2", id, transaction.command);
                d->notify(d->subscriptionId(id), NotifyType::delete_);
            }
            return;
        }

        invokeOnDbTypes<applyOr, Model>(
            [this, &transaction](auto&& data)
            {
                using T = std::decay_t<decltype(data)>;
                if (getUpdateCommand<T>() != transaction.command)
                    return false;

                auto d = static_cast<Derived*>(this);
                const auto id = getId(static_cast<const QnTransaction<T>&>(transaction).params);
                if (d->isValidType(id))
                {
                    NX_VERBOSE(this, "Notify %1 for %2", id, transaction.command);
                    d->notify(d->subscriptionId(id), NotifyType::update);
                }
                return true;
            });
    }

    nx::utils::Guard logTime(const char* label) const
    {
        return nx::utils::Guard(
            [this, label, timer = nx::utils::ElapsedTimer(nx::utils::ElapsedTimerState::started)]
            {
                NX_VERBOSE(this, "`%1` in `%2`", label, timer.elapsed());
            });
    }

private:
    static constexpr ApiObjectType m_objectType = details::commandToObjectType(DeleteCommand);
    nx::Lockable<nx::utils::TimeOutCache<nx::Uuid, nx::network::rest::CollectionHash>> m_etags{
        /*expirationPeriod*/ std::chrono::seconds{60}, /*maxSize*/ 100};
};

} // namespace ec2
