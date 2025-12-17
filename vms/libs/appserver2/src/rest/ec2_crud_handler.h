// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/network/rest/collection_hash.h>
#include <nx/network/rest/crud_handler.h>
#include <nx/network/rest/subscription.h>
#include <nx/utils/base64.h>
#include <nx/utils/crud_model.h>
#include <nx/utils/elapsed_timer.h>
#include <nx/utils/i18n/scoped_locale.h>
#include <nx/utils/lockable.h>
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
                    [this](const auto& transaction) { this->notify(transaction); }, std::move(commands));
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

        auto id = nx::utils::model::getId(filter);
        if (!request.jsonRpcContext() || !request.jsonRpcContext()->subscribed)
            validateType(id);

        auto processor = m_queryProcessor->getAccess(this->prepareAuditRecord(request));
        // `std::type_identity` (or similar) can be used to pass the read type info and avoid
        // useless `readType` object instantiation.
        const auto query =
            [this, &request, id, &processor](const auto& readType)
            {
                using Arg = std::decay_t<decltype(readType)>;
                using Output = std::conditional_t<nx::traits::is<std::vector, Arg>(), Arg, std::vector<Arg>>;
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
        if constexpr (requires { &Model::fromDbTypes; })
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

    void delete_(DeleteInput id, const Request& request)
    {
        using namespace details;
        using namespace nx::utils::model;

        validateType(getId(id));

        std::promise<Result> promise;
        auto processor = m_queryProcessor->getAccess(this->prepareAuditRecord(request));
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
        if constexpr (requires { &Model::toDbTypes; })
        {
            updateDbTypes(std::move(data).toDbTypes(), request);
        }
        else
        {
            validateType(nx::utils::model::getId(data));
            validateResourceTypeId(data);
            std::promise<Result> promise;
            auto processor = m_queryProcessor->getAccess(this->prepareAuditRecord(request));
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
            return id.isNull() ? QString("*") : subscriptionIdFromId(id);
        }

        if constexpr (requires { d->flexibleIdToId(*it); })
        {
            if (const auto id = d->flexibleIdToId(*it); id.isNull())
                throw Exception::notFound(NX_FMT("Resource '%1' is not found", *it));
            else
                return d->subscriptionIdFromId(id);
        }
        return subscriptionIdFromId(*it);
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
            nx::utils::fromBase64Url(etagIn), nx::utils::fromBase64Url(etagOut));
    }

protected:
    template<typename Data>
    nx::network::rest::CollectionHash calculateEtags(std::vector<Data>* list) {
        auto d = static_cast<Derived*>(this);
        std::vector<nx::network::rest::CollectionHash::Item> data;
        data.reserve(list->size());
        for (const auto& item: *list)
        {
            data.emplace_back(d->subscriptionIdFromId(nx::utils::model::getId(item)),
                nx::reflect::json::serialize(item));
        }
        auto result = nx::network::rest::CollectionHash{std::move(data)};
        if constexpr (requires { Data::etag; })
        {
            for (auto& item: *list)
            {
                item.etag = nx::utils::toBase64Url(
                    result.hash(d->subscriptionIdFromId(nx::utils::model::getId(item))));
            }
        }
        return result;
    }

    template<typename Id, typename Data>
    std::vector<Data> updateCombinedEtag(Id id, std::vector<Data> result,
        const Request& request, ResponseAttributes* responseAttributes)
    {
        if (!responseAttributes)
            return result;

        const bool processJsonRpcEtag = request.jsonRpcContext()
            && (request.jsonRpcContext()->subscribed
                || request.jsonRpcContext()->subs == nx::network::rest::json_rpc::Subs::subscribe);
        nx::network::rest::CollectionHash::Value etag;
        auto d = static_cast<Derived*>(this);
        if (id == Id{})
        {
            auto etags = calculateEtags(&result);
            etag = etags.combinedHash();
            if (processJsonRpcEtag)
            {
                {
                    auto lock = m_subscriptions.lock();
                    (*lock)[request.jsonRpcContext()->connection.id].etags = std::move(etags);
                }
                auto providedEtag =
                    nx::network::http::getHeaderValue(request.httpHeaders(), "If-None-Match");
                if (!providedEtag.empty()
                    && nx::network::rest::CollectionHash::check(
                        nx::network::rest::CollectionHash::Check::list,
                        nx::utils::fromBase64Url(providedEtag),
                        etag))
                {
                    result.clear();
                }
            }
        }
        else if (!result.empty())
        {
            nx::network::rest::CollectionHash::Item item{
                d->subscriptionIdFromId(id), nx::reflect::json::serialize(result.front())};
            if (processJsonRpcEtag)
            {
                auto lock = m_subscriptions.lock();
                if (auto it = lock->find(request.jsonRpcContext()->connection.id);
                    it != lock->end() && it->second.etags)
                {
                    bool changed = false;
                    std::tie(etag, changed) = it->second.etags->calculate(std::move(item));
                    // NOTE: Here we considering that item is not changed only for notifications.
                    // Initial subscription request for single item always sends item in response
                    // because of current limitations.
                    if (!changed && request.jsonRpcContext()->subscribed)
                    {
                        result.clear();
                    }
                    else
                    {
                        if constexpr (requires { Data::etag; })
                            result.front().etag = nx::utils::toBase64Url(etag);
                    }
                }
                else
                {
                    nx::network::rest::CollectionHash etags;
                    etag = etags.calculate(std::move(item)).first;
                }
            }
            else
            {
                nx::network::rest::CollectionHash etags;
                etag = etags.calculate(std::move(item)).first;
            }
        }
        if (!etag.empty())
        {
            nx::network::http::insertOrReplaceHeader(
                &responseAttributes->httpHeaders, {"ETag", nx::utils::toBase64Url(etag)});
        }
        return result;
    }

    template<typename T>
    void updateDbTypes(T items, const nx::network::rest::Request& request)
    {
        using namespace details;
        const auto& mainDbItem = std::get<0>(items);
        auto id = nx::utils::model::getId(mainDbItem);
        validateType(id);
        validateResourceTypeId(mainDbItem);
        static_cast<Derived*>(this)->checkSavePermission(request.userSession.access, mainDbItem);
        using IgnoredDbType = std::decay_t<decltype(mainDbItem)>;
        auto update =
            [id](auto&& x, auto& copy, auto* list)
            {
                using DbType = std::decay_t<decltype(x)>;
                Result result;
                assertModelToDbTypesProducedValidResult<IgnoredDbType, DbType, Model>(x, id);
                if constexpr (nx::traits::is<std::vector, DbType>())
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
        auto processor = m_queryProcessor->getAccess(this->prepareAuditRecord(request));
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
                        if constexpr (nx::traits::is<std::optional, std::decay_t<decltype(item)>>())
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

    static QString subscriptionIdFromId(QString id)  { return id; }
    static QString subscriptionIdFromId(nx::Uuid id)  { return id.toSimpleString(); }

    void notify(
        const QString& id,
        NotifyType notifyType,
        std::optional<nx::Uuid> user = {},
        bool noEtag = false)
    {
        constexpr int kNone = 0;
        constexpr int kAll = 1 << 0;
        constexpr int kOne = 1 << 1;
        constexpr int kBoth = kAll | kOne;
        auto ignore =
            [&](const auto& connection)
            {
                auto lock = m_subscriptions.lock();
                auto it = lock->find(connection);
                if (it == lock->end())
                    return kBoth;

                int r = kNone;
                for (auto [id_, ignore]: {std::pair{id, kOne}, std::pair{QString("*"), kAll}})
                {
                    auto n = it->second.notifications.find(id_);
                    if (n == it->second.notifications.end())
                    {
                        r |= ignore;
                        continue;
                    }
                    if (n->second)
                    {
                        auto& v = *n->second;
                        v.push_back({id, notifyType, user, noEtag});
                        if (const auto size = v.size(); size > 1 && v[size - 1] == v[size - 2])
                            v.pop_back();
                        r |= ignore;
                    }
                }
                return r;
            };
        switch (notifyType)
        {
            case NotifyType::update:
            {
                using List = typename nx::traits::FunctionTraits<&Derived::read>::ReturnType;
                using Data = typename List::value_type;
                auto callbacks = SubscriptionHandler::callbacks(id, std::move(user),
                    [this](const auto& request) { return makePostProcessContext(request); });
                for (auto& [user, connections]: callbacks)
                {
                    for (auto& [connection, callbacks]: connections)
                    {
                        const int ignore_ = ignore(connection.id);
                        if (ignore_ == kBoth)
                            continue;

                        for (auto& [apiVersion, subscriptionId, callback, postProcess]: callbacks)
                        {
                            if ((ignore_ == kOne && subscriptionId != "*")
                                || (ignore_ == kAll && subscriptionId == "*"))
                            {
                                continue;
                            }

                            auto payload =
                                payloadForUpdate<Data>(apiVersion, id, user, noEtag, connection);
                            if (!payload.data)
                                continue;

                            rapidjson::Document extensions;
                            if (payload.extensions)
                            {
                                extensions = nx::json::serialized(
                                    *payload.extensions, /*stripDefault*/ false);
                            }
                            auto& data = *payload.data;
                            nx::network::rest::detail::filter(&data, postProcess.filters);
                            nx::network::rest::detail::orderBy(&data, postProcess.filters);
                            auto document = nx::network::rest::json::serialize(data,
                                std::move(postProcess.filters),
                                postProcess.defaultValueAction);
                            this->m_schemas->postprocessResponse(
                                postProcess.method, postProcess.path, &document);
                            (*callback)(id, notifyType, &document, &extensions);
                        }
                    }
                }
                break;
            }
            case NotifyType::delete_:
            {
                auto callbacks = SubscriptionHandler::callbacks(id, std::move(user));
                for (auto& [user, connections]: callbacks)
                {
                    for (auto& [connection, callbacks]: connections)
                    {
                        const int ignore_ = ignore(connection.id);
                        if (ignore_ == kBoth)
                            continue;

                        auto data = nx::json::serialized(DeleteInput{id}, /*stripDefault*/ false);
                        rapidjson::Document extensions;
                        {
                            auto lock = m_subscriptions.lock();
                            if (auto it = lock->find(connection.id);
                                it != lock->end() && it->second.etags)
                            {
                                auto etag = it->second.etags->remove(id);
                                if (!noEtag)
                                {
                                    extensions = nx::json::serialized(
                                        nx::network::rest::json_rpc::ClientExtensions{
                                            nx::utils::toBase64Url(etag)},
                                        /*stripDefault*/ false);
                                }
                            }
                        }
                        for (auto& [subscriptionId, callback]: callbacks)
                        {
                            if (ignore_ == kNone
                                || (ignore_ == kOne && subscriptionId == "*")
                                || (ignore_ == kAll && subscriptionId != "*"))
                            {
                                (*callback)(id, notifyType, &data, &extensions);
                            }
                        }
                    }
                }
                break;
            }
        }
    }

protected:
    QueryProcessor* const m_queryProcessor;

private:
    template<typename T>
    void validateType(const T& id)
    {
        if (!static_cast<Derived*>(this)->isValidType(id))
        {
            throw nx::network::rest::Exception::notFound(
                NX_FMT("Object type %1 is not found", nx::reflect::json::serialize(id)));
        }
    }

    template<typename T>
    nx::network::rest::json_rpc::Payload<T> payloadForUpdate(
        size_t apiVersion,
        const QString& id,
        const nx::Uuid& user,
        bool noEtag,
        nx::network::rest::json_rpc::WeakConnection connection)
    {
        using namespace nx::network::rest;
        json_rpc::Payload<T> result;
        try
        {
            ResponseAttributes responseAttributes;
            auto list = static_cast<Derived*>(this)->read({id},
                payloadRequest(id, user, apiVersion, std::move(connection)),
                &responseAttributes);
            if (!list.empty())
            {
                result.data = std::move(list.front());
                if (!noEtag)
                {
                    if (auto it = responseAttributes.httpHeaders.find("ETag");
                        it != responseAttributes.httpHeaders.end())
                    {
                        result.extensions.emplace().etag = it->second;
                    }
                }
            }
        }
        catch (const Exception& e)
        {
            NX_VERBOSE(this, "Failed read() for notification %1: %2", id, e);
        }
        return result;
    }

    PostProcessContext makePostProcessContext(const nx::network::rest::Request& request) const
    {
        PostProcessContext result{request.params(), request.method(), request.decodedPath()};
        if (!result.path.endsWith('}'))
            result.path += "/{" + this->m_idParamName + '}';
        for (const auto f: nx::reflect::listFieldNames<std::decay_t<Filter>>())
            result.filters.remove(QString::fromStdString(std::string{f}));
        if constexpr (requires { Filter::getDeprecatedFieldNames(); })
        {
            auto names = Filter::getDeprecatedFieldNames();
            for (auto it = names->begin(); it != names->end(); ++it)
                result.filters.remove(it.value());
        }
        result.defaultValueAction =
            this->extractDefaultValueAction(&result.filters, request.apiVersion());
        return result;
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
                d->notify(d->subscriptionIdFromId(id), NotifyType::delete_);
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
                    d->notify(d->subscriptionIdFromId(id), NotifyType::update);
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

    virtual nx::utils::Guard subscribe(
        nx::network::rest::Handler::SubscriptionResponseHandler handler,
        Request request,
        SubscriptionCallback callback) override
    {
        using namespace nx::network::rest;
        auto subscriptionId = request.jsonRpcContext()->subscriptionId;
        auto connection = request.jsonRpcContext()->connection.id;
        nx::utils::Guard ownGuard{
            [this, subscriptionId, connection]
            {
                auto lock = m_subscriptions.lock();
                if (auto it = lock->find(connection); it != lock->end())
                {
                    it->second.notifications.erase(subscriptionId);
                    if (subscriptionId == "*")
                        it->second.etags.reset();
                    if (it->second.notifications.empty())
                        lock->erase(it);
                }
            }};

        // Prepare place to collect subscription notifications. Turn of usual notification
        // processing.
        auto processor = m_queryProcessor->getAccess(this->prepareAuditRecord(request));
        {
            std::promise<void> p;
            std::future<void> f = p.get_future();
            processor.processCustomUpdateAsync(ApiCommand::NotDefined,
                [p = std::move(p)](auto&&...) mutable { p.set_value(); },
                [this, subscriptionId, connection](auto& /*copy*/, auto list)
                {
                    list->push_back(
                        [this, subscriptionId, connection]()
                        {
                            {
                                auto lock = m_subscriptions.lock();
                                auto& s = (*lock)[connection];
                                s.notifications[subscriptionId] = Notifications{};
                            }
                            NX_VERBOSE(this,
                                "Start ignoring subscription %1 for connection %2",
                                subscriptionId, connection);
                        });
                    return Result();
                });
            f.wait();
        }

        // Add subscription - start collection of notifications instead of sending them to
        // connection.
        auto guard = static_cast<Derived*>(this)->addSubscription(
            {json_rpc::Context{
                request.jsonRpcContext()->request.copy(),
                request.jsonRpcContext()->connection,
                request.jsonRpcContext()->subscribed,
                request.jsonRpcContext()->subscriptionId,
                request.jsonRpcContext()->crud,
                request.jsonRpcContext()->subs},
                request.userSession},
            std::move(callback));

        // Send subscribe response to connection. NOTE: executeGet must be after addSubscription.
        handler(nx::network::rest::json_rpc::to(
            request.jsonRpcContext()->request.responseId(), this->executeGet(request)));

        // Send collected notifications if any and switch to usual notification processing.
        processor.processCustomUpdateAsync(ApiCommand::NotDefined,
            [](auto&&...) {},
            [this, subscriptionId, connection](auto& /*copy*/, auto list)
            {
                list->push_back(
                    [this, subscriptionId, connection]()
                    {
                        Notifications notifications;
                        {
                            auto lock = m_subscriptions.lock();
                            if (auto it = lock->find(connection); it != lock->end())
                            {
                                auto n = it->second.notifications.find(subscriptionId);
                                if (n != it->second.notifications.end() && n->second)
                                {
                                    notifications = std::move(*n->second);
                                    n->second.reset();
                                }
                            }
                        }
                        NX_VERBOSE(this,
                            "Stop ignoring subscription %1 for connection %2, collected %3",
                            subscriptionId, connection, notifications.size());
                        auto d = static_cast<Derived*>(this);
                        for (auto& n: notifications)
                        {
                            std::apply(
                                [d](auto&&... args)
                                {
                                    d->notify(std::forward<decltype(args)>(args)...);
                                },
                                std::move(n));
                        }
                    });
                return Result();
            });
        return {[g1 = std::move(guard), g2 = std::move(ownGuard)]() {}};
    }

private:
    struct Subscriptions
    {
        using Notification = std::tuple<QString, NotifyType, std::optional<nx::Uuid>, bool>;
        using Notifications = std::vector<Notification>;
        std::map<QString, std::optional<Notifications>> notifications;
        std::optional<nx::network::rest::CollectionHash> etags;
    };

    using Notifications = Subscriptions::Notifications;

private:
    static constexpr ApiObjectType m_objectType = details::commandToObjectType(DeleteCommand);
    nx::Lockable<std::map<size_t, Subscriptions>> m_subscriptions;
};

} // namespace ec2
