// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <type_traits>
#include <utility>

#include <QtCore/QString>

#include <nx/fusion/model_functions_fwd.h>
#include <nx/utils/member_detector.h>
#include <nx/utils/type_traits.h>
#include <nx/utils/void.h>

#include "handler.h"
#include "json.h"
#include "params.h"
#include "request.h"
#include "response.h"

namespace nx::network::rest {

enum class CrudFeature
{
    idInPost = 1 << 0, //< Allow providing id parameter in POST request.
    alwaysKeepDefault = 1 << 1, //< Keep default values in response regardless of _keepDefault.
    fastUpdate = 1 << 2, //< Execute update on PUT and PATCH without read.
};
Q_DECLARE_FLAGS(CrudFeatures, CrudFeature)

struct ResponseAttributes
{
    nx::network::http::HttpHeaders httpHeaders;
    nx::network::http::StatusCode::Value statusCode = nx::network::http::StatusCode::ok;
};

template<typename Derived>
class CrudHandler: public Handler
{
public:
    using Void = nx::utils::Void;

    explicit CrudHandler(QString idParamName = "id", CrudFeatures features = {}):
        // Maximum security if not set on registration.
        Handler(GlobalPermission::administrator, GlobalPermission::administrator),
        m_idParamName(std::move(idParamName)),
        m_features(features)
    {
    }

    explicit CrudHandler(CrudFeatures features): CrudHandler("id", features) {}

protected:
    virtual void modifyPathRouteResultOrThrow(PathRouter::Result* result) const override
    {
        if constexpr (DoesMethodExist_flexibleIdToId<Derived>::value)
        {
            const auto it = result->pathParams.findValue(m_idParamName);
            if (!it || (nx::Uuid::isUuidString(*it) && !nx::Uuid(*it).isNull()))
                return;
            const nx::Uuid id = static_cast<const Derived*>(this)->flexibleIdToId(*it);
            if (id.isNull())
                throw Exception::notFound(NX_FMT("Resource '%1' is not found", *it));
            result->pathParams.replace(m_idParamName, id.toString());
        }
    }

    // TODO:
    //   Consider moving this logic into the `Request` class.
    //   For example, Request's constructor can be injected with this validation logic.
    //   Request object should have maximum immutability.
    //   Handler should be agnostic of the actual URI path. It only expects particular parameters.
    virtual void clarifyApiVersionOrThrow(const PathRouter::Result& result, Request* request) const override
    {
        if (result.validationPath.startsWith("/rest/"))
        {
            auto name = result.validationPath.split('/')[2];
            if (!name.startsWith('v'))
                throw Exception::notFound(NX_FMT("API version '%1' is not supported", name));

            bool isOk = false;
            const auto number = name.mid(1).toInt(&isOk);
            if (!isOk || number <= 0)
                throw Exception::notFound(NX_FMT("API version '%1' is not supported", name));

            request->setApiVersion((size_t) number);
        }
    }

    virtual bool isConcreteIdProvidedInPath(PathRouter::Result* result) const override
    {
        auto it = result->pathParams.findValue(m_idParamName);
        return it && *it != NX_FMT("*");
    }

    template<typename T>
    struct MethodArgument0
    {
    };

    template<typename Result, typename Argument0, typename Class, typename... Args>
    struct MethodArgument0<Result (Class::*)(Argument0, Args...)>
    {
        using type = Argument0;
    };

    template<typename T>
    struct MethodResult
    {
    };

    template<typename Result, typename Class, typename... Args>
    struct MethodResult<Result (Class::*)(Args...)>
    {
        using type = Result;
    };

    #define METHOD_CHECKER(METHOD) \
        NX_UTILS_DECLARE_FIELD_DETECTOR_SIMPLE(DoesMethodExist_##METHOD, METHOD)
    METHOD_CHECKER(loadFromParams);
    METHOD_CHECKER(size);
    METHOD_CHECKER(toParams);
    METHOD_CHECKER(create);
    METHOD_CHECKER(read);
    METHOD_CHECKER(delete_);
    METHOD_CHECKER(update);
    METHOD_CHECKER(addSubscription);
    METHOD_CHECKER(getSubscriptionId);
    METHOD_CHECKER(flexibleIdToId);
    METHOD_CHECKER(fillMissingParams);
    METHOD_CHECKER(fillMissingParamsForResponse);
    #undef METHOD_CHECKER

    virtual Response executeGet(const Request& request) override;
    virtual Response executePatch(const Request& request) override;
    virtual Response executePost(const Request& request) override;
    virtual Response executePut(const Request& request) override;
    virtual Response executeDelete(const Request& request) override;
    virtual QString subscriptionId(const Request& request) override;
    virtual nx::utils::Guard subscribe(const QString& id, SubscriptionCallback callback) override;
    virtual QString idParamName() const override { return m_idParamName; }

    template<typename OutputData>
    QByteArray serializeOrThrow(const OutputData& outputData, Qn::SerializationFormat format)
    {
        if (format == Qn::SerializationFormat::urlEncoded
            || format == Qn::SerializationFormat::urlQuery)
        {
            if (!outputData.isObject())
            {
                throw nx::network::rest::Exception::unsupportedMediaType(
                    NX_FMT("Unsupported format for non-object data: %1", format));
            }

            const auto encode = format == Qn::SerializationFormat::urlEncoded
                ? QUrl::FullyEncoded
                : QUrl::PrettyDecoded;
            return Params::fromJson(outputData.toObject()).toUrlQuery().query(encode).toUtf8();
        }
        auto serializedData = trySerialize(outputData, format);
        if (!serializedData.has_value())
        {
            throw nx::network::rest::Exception::unsupportedMediaType(
                NX_FMT("Unsupported format: %1", format));
        }
        return *serializedData;
    }

    template<typename T>
    T deserialize(const Request& request, bool wrapInObject = false)
    {
        T data;
        if constexpr (DoesMethodExist_loadFromParams<T>::value)
        {
            data.loadFromParams(static_cast<Derived*>(this)->resourcePool(), request.params());
            if (!data.isValid())
                throw Exception::badRequest(NX_FMT("Invalid: %1", typeid(T)));
            if (!request.params().contains("local"))
                data.isLocal = request.isLocal();
            if (!request.params().contains("format"))
                data.format = request.responseFormatOrThrow();
        }
        else if constexpr (!std::is_same<T, Void>::value)
        {
            data = request.parseContentOrThrow<T>(wrapInObject);
        }
        return data;
    }

    template<typename T>
    struct ArgumentCount;

    template<typename Result, typename Object, typename... Args>
    struct ArgumentCount<Result(Object::*)(Args...)>
    {
        static constexpr auto value = sizeof...(Args);
    };

    template<typename Method, typename Model>
    auto call(Method method, Model model, const Request& request, ResponseAttributes* responseAttributes)
    {
        auto d = static_cast<Derived*>(this);
        if constexpr (ArgumentCount<Method>::value == 3)
            return (d->*method)(std::move(model), request, responseAttributes);
        else
            return (d->*method)(std::move(model), request);
    }

    template<typename Data>
    Response response(
        const Data& data, const Request& request, ResponseAttributes responseAttributes = {},
        Params filters = {}, json::DefaultValueAction defaultValueAction = json::DefaultValueAction::appendMissing)
    {
        auto json = json::filter(data, std::move(filters), defaultValueAction);
        if (NX_ASSERT(m_schemas))
            m_schemas->postprocessResponse(request, &json);
        const auto format = request.responseFormatOrThrow();
        Response response(responseAttributes.statusCode, std::move(responseAttributes.httpHeaders));
        response.content = {
            {Qn::serializationFormatToHttpContentType(format)}, serializeOrThrow(json, format)};
        return response;
    }

    template<typename Id>
    Response responseById(Id id, const Request& request, ResponseAttributes responseAttributes = {})
    {
        if (auto resource = readById(id, request, &responseAttributes))
        {
            if constexpr (DoesMethodExist_fillMissingParamsForResponse<Derived>::value)
                static_cast<Derived*>(this)->fillMissingParamsForResponse(&*resource, request);
            return response(*resource, request, std::move(responseAttributes));
        }

        // This may actually happen if created resource is removed faster than the handler
        // generated a response.
        throw Exception::internalServerError(NX_FMT("Resource '%1' is not found", id));
    }

    template<typename Id>
    auto readById(Id id, const Request& request, ResponseAttributes* responseAttributes)
    {
        auto list = call(&Derived::read, std::move(id), request, responseAttributes);
        using ResultType = std::optional<std::decay_t<decltype(list.front())>>;
        if (list.empty())
            return ResultType(std::nullopt);
        if (const auto size = list.size(); size != 1)
        {
            const auto error =
                NX_FMT("There are %1 resources with '%2' id while it should be one.", size, id);
            NX_ASSERT(false, error);
            throw Exception::internalServerError(error);
        }
        return ResultType(std::move(list.front()));
    }

    struct IdParam
    {
        std::optional<QString> value;
        bool isInPath = false;
    };

    IdParam idParam(const Request& request) const
    {
        const auto& params = request.params();
        const auto id = params.findValue(m_idParamName);

        if (!id || !request.isConcreteIdProvided())
            return {};

        return {id, request.pathParams().contains(m_idParamName)};
    }

    bool extractKeepDefault(Params* params, bool keepDefaultFeature, bool defaultBehavior)
    {
        const auto extractValue =
            [&params](const QString& name) -> std::optional<bool>
            {
                const auto value = params->findValue(name);
                if (!value)
                    return std::nullopt;
                params->remove(name);
                return (*value != "false" && *value != "0");
            };

        const auto keepDefault = extractValue("_keepDefault");
        const auto stripDefault = extractValue("_stripDefault");
        if (keepDefaultFeature)
            return true; //< And ignore stripped options.
        if (!keepDefault)
            return stripDefault ? !*stripDefault : defaultBehavior;
        if (stripDefault)
            throw Exception::badRequest("Only one can be used: _keepDefault, _stripDefault");
        return keepDefault.value_or(defaultBehavior);
    }

    json::DefaultValueAction extractDefaultValueAction(
        Params* params, std::optional<size_t> apiVersion) const
    {
        const auto extractBoolParam =
            [&](const QString& name) -> std::optional<bool>
            {
                const auto value = params->findValue(name);
                if (!value)
                    return std::nullopt;
                params->remove(name);
                return (*value != "false" && *value != "0");
            };

        const auto extractBoolValue =
            [&](bool defaultBehavior) -> bool
            {
                const auto keepDefault = extractBoolParam("_keepDefault");
                const auto stripDefault = extractBoolParam("_stripDefault");
                if (m_features.testFlag(CrudFeature::alwaysKeepDefault))
                    return true; //< And ignore extracted values.
                if (!keepDefault)
                    return stripDefault ? !*stripDefault : defaultBehavior;
                if (stripDefault)
                    throw Exception::badRequest("Only one can be used: _keepDefault, _stripDefault");
                return keepDefault.value_or(defaultBehavior);
            };

        return extractBoolValue(/*defaultBehavior*/ apiVersion.value_or(0) >= 3)
            ? json::DefaultValueAction::appendMissing
            : json::DefaultValueAction::removeEqual;
    }

protected:
    const QString m_idParamName;
    const CrudFeatures m_features;
};

template<typename Derived>
Response CrudHandler<Derived>::executeGet(const Request& request)
{
    if constexpr (DoesMethodExist_read<Derived>::value)
    {
        auto* d = static_cast<Derived*>(this);
        using Filter = typename MethodArgument0<decltype(&Derived::read)>::type;
        Filter filter = request.params().toJson(/*excludeCommon*/ true).isEmpty()
            ? Filter()
            : deserialize<Filter>(request);
        Params filtered;
        if constexpr (DoesMethodExist_toParams<Filter>::value)
        {
            filtered = filter.toParams();
            filtered.insert("local", QString());
            filtered.insert("format", QString());
            filtered.insert("extraFormatting", QString());
        }
        else
        {
            QnJsonContext jsonContext;
            jsonContext.setChronoSerializedAsDouble(true);
            QJsonValue intermediate;
            QJson::serialize(&jsonContext, filter, &intermediate);
            if (intermediate.isObject())
                filtered = Params::fromJson(intermediate.toObject());
        }

        ResponseAttributes responseAttributes;
        auto list = call(&Derived::read, std::move(filter), request, &responseAttributes);
        if constexpr (DoesMethodExist_fillMissingParamsForResponse<Derived>::value)
        {
            for (auto& model: list)
                d->fillMissingParamsForResponse(&model, request);
        }
        auto params = request.params();
        for (auto [key, value]: filtered.keyValueRange())
            params.remove(key);

        const auto defaultValueAction = extractDefaultValueAction(&params, request.apiVersion());
        if (const auto id = idParam(request)
            ;
            (id.value && id.isInPath) || m_idParamName.isEmpty())
        {
            const auto size = list.size();
            if (size == 1)
            {
                return response(list.front(),
                    request, std::move(responseAttributes), std::move(params), defaultValueAction);
            }

            if (size == 0)
            {
                if (id.value)
                    throw Exception::notFound(NX_FMT("Resource '%1' is not found", *(id.value)));
                else
                    throw Exception::notFound("Resource is not found");
            }
            const auto error = NX_FMT("There are %1 resources while it should be one.", size);
            NX_DEBUG(this, error);
            if (id.value)
                throw Exception::invalidParameter(*id.value, error);
            else
                throw Exception::internalServerError(error);
        }
        return response(list,
            request, std::move(responseAttributes), std::move(params), defaultValueAction);
    }
    else
    {
        return nx::network::http::StatusCode::notImplemented;
    }
}

template<typename Derived>
Response CrudHandler<Derived>::executePost(const Request& request)
{
    if constexpr (DoesMethodExist_create<Derived>::value)
    {
        const auto derived = static_cast<Derived*>(this);
        if (!m_features.testFlag(CrudFeature::idInPost) && idParam(request).value)
        {
            throw Exception::badRequest(
                NX_FMT("The parameter %1 must not be specified when creating a new object.",
                    m_idParamName));
        }
        using Model = typename MethodArgument0<decltype(&Derived::create)>::type;
        Model model = deserialize<Model>(request);
        if constexpr (DoesMethodExist_fillMissingParams<Derived>::value)
            derived->fillMissingParams(&model, request);

        if constexpr (nx::utils::isCreateModelV<Model>)
        {
            if (auto id = model.getId(); id == decltype(id)())
            {
                model.setId(nx::Uuid::createUuid());
            }
            else if constexpr (DoesMethodExist_read<Derived>::value)
            {
                ResponseAttributes responseAttributes;
                if (call(&Derived::read, std::move(id), request, &responseAttributes).size() == 1)
                    throw Exception::forbidden("Already exists");
            }
        }

        ResponseAttributes responseAttributes;
        using Result = typename MethodResult<decltype(&Derived::create)>::type;
        if constexpr (!std::is_same<Result, void>::value)
        {
            const auto result = call(&Derived::create, std::move(model), request, &responseAttributes);
            return response(result, request, std::move(responseAttributes));
        }
        else if constexpr (DoesMethodExist_read<Derived>::value
            && nx::utils::HasGetId<Model>::value)
        {
            auto id = model.getId();
            call(&Derived::create, std::move(model), request, &responseAttributes);
            return responseById(std::move(id), request, std::move(responseAttributes));
        }
        else
        {
            call(&Derived::create, std::move(model), request, &responseAttributes);
            return {responseAttributes.statusCode, std::move(responseAttributes.httpHeaders)};
        }
    }
    else
    {
        return nx::network::http::StatusCode::notImplemented;
    }
}

template<typename Derived>
Response CrudHandler<Derived>::executeDelete(const Request& request)
{
    if constexpr (DoesMethodExist_delete_<Derived>::value)
    {
        ResponseAttributes responseAttributes;
        using Id = typename MethodArgument0<decltype(&Derived::delete_)>::type;
        if constexpr (std::is_same<Id, Void>::value)
        {
            call(&Derived::delete_, Void(), request, &responseAttributes);
        }
        else
        {
            Id id = deserialize<Id>(request);
            if constexpr (nx::utils::HasGetId<Id>::value && !DoesMethodExist_loadFromParams<Id>::value)
            {
                if (id.getId() == std::decay_t<decltype(id.getId())>())
                    throw Exception::missingParameter(m_idParamName);
            }
            call(&Derived::delete_, std::move(id), request, &responseAttributes);
        }
        return {responseAttributes.statusCode, std::move(responseAttributes.httpHeaders)};
    }
    else
    {
        return nx::network::http::StatusCode::notImplemented;
    }
}

template<typename Derived>
Response CrudHandler<Derived>::executePut(const Request& request)
{
    if constexpr (DoesMethodExist_update<Derived>::value)
    {
        const auto d = static_cast<Derived*>(this);
        using Model = typename MethodArgument0<decltype(&Derived::update)>::type;
        using Result = typename MethodResult<decltype(&Derived::update)>::type;
        Model model = deserialize<Model>(request, /*wrapInObject*/ true);
        if constexpr (DoesMethodExist_fillMissingParams<Derived>::value)
            d->fillMissingParams(&model, request);
        ResponseAttributes responseAttributes;
        if (idParam(request).value || m_idParamName.isEmpty())
        {
            auto id = model.getId();
            if (id == decltype(id)())
                throw Exception::missingParameter(m_idParamName);
            if constexpr (!std::is_same<Result, void>::value)
            {
                const auto result =
                    call(&Derived::update, std::move(model), request, &responseAttributes);
                if constexpr (DoesMethodExist_size<Result>::value)
                {
                    if (NX_ASSERT(result.size() == 1, "Expected 1 result, got %1", result.size()))
                        return response(result.front(), request, std::move(responseAttributes));
                    return {
                        responseAttributes.statusCode, std::move(responseAttributes.httpHeaders)};
                }
                else
                {
                    return response(result, request, std::move(responseAttributes));
                }
            }
            else
            {
                call(&Derived::update, std::move(model), request, &responseAttributes);
                if constexpr (DoesMethodExist_read<Derived>::value)
                {
                    if (!m_features.testFlag(CrudFeature::fastUpdate))
                        return responseById(std::move(id), request, std::move(responseAttributes));
                }
                return {responseAttributes.statusCode, std::move(responseAttributes.httpHeaders)};
            }
        }

        if constexpr (!std::is_same<Result, void>::value)
        {
            const auto result = call(&Derived::update, std::move(model), request, &responseAttributes);
            return response(result, request, std::move(responseAttributes));
        }
        else
        {
            call(&Derived::update, std::move(model), request, &responseAttributes);
            if constexpr (DoesMethodExist_read<Derived>::value)
            {
                if (!m_features.testFlag(CrudFeature::fastUpdate))
                {
                    auto emptyFilter = typename MethodArgument0<decltype(&Derived::read)>::type{};
                    const auto result =
                        call(&Derived::read, std::move(emptyFilter), request, &responseAttributes);
                    return response(result, request, std::move(responseAttributes));
                }
            }
            return {responseAttributes.statusCode, std::move(responseAttributes.httpHeaders)};
        }
    }
    else
    {
        return nx::network::http::StatusCode::notImplemented;
    }
}

template<typename Derived>
Response CrudHandler<Derived>::executePatch(const Request& request)
{
    if constexpr (DoesMethodExist_read<Derived>::value && DoesMethodExist_update<Derived>::value)
    {
        const auto d = static_cast<Derived*>(this);
        using Model = typename MethodArgument0<decltype(&Derived::update)>::type;
        using Result = typename MethodResult<decltype(&Derived::update)>::type;
        std::optional<QJsonValue> incomplete;
        Model model = request.parseContentAllowingOmittedValuesOrThrow<Model>(&incomplete);
        ResponseAttributes responseAttributes;
        if (idParam(request).value || m_idParamName.isEmpty())
        {
            auto id = model.getId();
            if (id == decltype(id)())
                throw Exception::missingParameter(m_idParamName);

            if (incomplete && !m_features.testFlag(CrudFeature::fastUpdate))
            {
                const auto systemAccessGuard = request.forceSystemAccess();
                const auto existing = readById(id, request, &responseAttributes);
                if (!existing)
                    throw Exception::notFound(NX_FMT("Resource '%1' is not found", id));
                QString error;
                if (!json::merge(
                    &model, *existing, *incomplete, &error, /*chronoSerializedAsDouble*/ true))
                {
                    throw Exception::badRequest(error);
                }
            }

            if constexpr (DoesMethodExist_fillMissingParams<Derived>::value)
                d->fillMissingParams(&model, request);
            if constexpr (!std::is_same<Result, void>::value)
            {
                const auto result =
                    call(&Derived::update, std::move(model), request, &responseAttributes);
                return response(result, request, std::move(responseAttributes));
            }
            else
            {
                call(&Derived::update, std::move(model), request, &responseAttributes);
                if (!m_features.testFlag(CrudFeature::fastUpdate))
                    return responseById(std::move(id), request, std::move(responseAttributes));
                return {responseAttributes.statusCode, std::move(responseAttributes.httpHeaders)};
            }
        }

        auto emptyFilter = model.getId();
        if (!m_features.testFlag(CrudFeature::fastUpdate))
        {
            if (!incomplete)
            {
                QJsonValue serialized;
                QJson::serialize(model, &serialized);
                incomplete = std::move(serialized);
            }
            const auto systemAccessGuard = request.forceSystemAccess();
            QString error;
            if (!json::merge(
                &model,
                call(&Derived::read, emptyFilter, request, &responseAttributes),
                *incomplete,
                &error,
                /*chronoSerializedAsDouble*/ true))
            {
                throw Exception::badRequest(error);
            }
        }
        if constexpr (DoesMethodExist_fillMissingParams<Derived>::value)
            d->fillMissingParams(&model, request);

        if constexpr (!std::is_same<Result, void>::value)
        {
            const auto result =
                call(&Derived::update, std::move(model), request, &responseAttributes);
            return response(result, request, std::move(responseAttributes));
        }
        else
        {
            call(&Derived::update, std::move(model), request, &responseAttributes);
            if (m_features.testFlag(CrudFeature::fastUpdate))
                return {responseAttributes.statusCode, std::move(responseAttributes.httpHeaders)};
            const auto result = call(&Derived::read, emptyFilter, request, &responseAttributes);
            return response(result, request, std::move(responseAttributes));
        }
    }
    else
    {
        return nx::network::http::StatusCode::notImplemented;
    }
}

template<typename Derived>
nx::utils::Guard CrudHandler<Derived>::subscribe(const QString& id, SubscriptionCallback callback)
{
    NX_ASSERT(!id.isEmpty(), "Id must be obtained by subscriptionId()");
    if constexpr (DoesMethodExist_addSubscription<Derived>::value)
    {
        return static_cast<Derived*>(this)->addSubscription(id, std::move(callback));
    }
    else
    {
        NX_ASSERT(
            false, "Check that handler supports subscriptions using subscriptionId().isNull()");
        return {};
    }
}

template<typename Derived>
QString CrudHandler<Derived>::subscriptionId(const Request& request)
{
    if constexpr (DoesMethodExist_getSubscriptionId<Derived>::value)
        return static_cast<Derived*>(this)->getSubscriptionId(request);
    else
        return {};
}

} // namespace nx::network::rest
