// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/network/rest/crud_handler.h>
#include <nx/network/rest/subscription.h>
#include <nx/utils/crud_model.h>
#include <nx/utils/elapsed_timer.h>
#include <nx/utils/i18n/scoped_locale.h>
#include <nx/utils/scope_guard.h>
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
    public nx::network::rest::SubscriptionHandler<std::shared_ptr<rapidjson::Document>>
{
public:
    using base_type = nx::network::rest::CrudHandler<Derived>;

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

    std::vector<Model> read(Filter filter, const nx::network::rest::Request& request)
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

        if constexpr (fromDbTypesExists<Model>::value)
        {
            const auto logGuard = logTime("Query and Convert from DB types");

            auto futures = std::apply(
                [&](const auto&... readTypes)
                {
                    return std::make_tuple(query(readTypes)...);
                },
                typename Model::DbReadTypes());
            return Model::fromDbTypes(
                std::apply(
                    [readFuture](auto&&... futures) -> typename Model::DbListTypes
                    {
                        return {readFuture(std::forward<decltype(futures)>(futures))...};
                    },
                    std::move(futures)));
        }
        else
        {
            return readFuture(query(Model()));
        }
    }

    void delete_(DeleteInput id, const nx::network::rest::Request& request)
    {
        using namespace details;

        auto processor = m_queryProcessor->getAccess(
            static_cast<const Derived*>(this)->prepareAuditRecord(request));
        validateType(processor, nx::utils::model::getId(id), m_objectType);

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

        if (auto c = request.jsonRpcContext(); c && c->crud == json_rpc::Crud::all)
            return QString("*");

        if (nx::Uuid::isUuidString(*it))
        {
            const auto id = nx::Uuid::fromStringSafe(*it);
            return id.isNull() ? QString("*") : nx::toString(id);
        }

        if constexpr (requires(Derived* d) { d->flexibleIdToId(*it); })
        {
            if (const auto id = static_cast<Derived*>(this)->flexibleIdToId(*it); id.isNull())
                throw Exception::notFound(NX_FMT("Resource '%1' is not found", *it));
            else
                return nx::toString(id);
        }
        else
        {
            return *it;
        }
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

protected:
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

        const auto objectType =
            m_queryProcessor->getAccess(nx::network::rest::kSystemSession).getObjectType(id);
        return objectType == ApiObject_NotDefined || objectType == m_objectType;
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
            const auto id =
                getId(static_cast<const QnTransaction<DeleteInput>&>(transaction).params);

            if (static_cast<Derived*>(this)->isValidType(id))
            {
                NX_VERBOSE(this, "Notify %1 for %2", id, transaction.command);
                SubscriptionHandler::notify(nx::toString(id), NotifyType::delete_, /*data*/ {});
            }
            return;
        }

        invokeOnDbTypes<applyOr, Model>(
            [this, &transaction](auto&& data)
            {
                using T = std::decay_t<decltype(data)>;
                if (getUpdateCommand<T>() != transaction.command)
                    return false;

                const auto id = getId(static_cast<const QnTransaction<T>&>(transaction).params);
                if (static_cast<Derived*>(this)->isValidType(id))
                {
                    NX_VERBOSE(this, "Notify %1 for %2", id, transaction.command);
                    SubscriptionHandler::notify(nx::toString(id), NotifyType::update, /*data*/ {});
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
};

} // namespace ec2
