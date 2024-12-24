// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/json_rpc/messages.h>
#include <nx/reflect/json.h>
#include <nx/reflect/merge.h>
#include <nx/reflect/urlencoded/deserializer.h>
#include <nx/utils/void.h>

#include "exception.h"
#include "json.h"
#include "nx_network_rest_ini.h"
#include "params.h"
#include "user_access_data.h"

namespace nx::json_rpc { class WebSocketConnection; }

namespace nx::network::rest {

namespace audit { struct Record; }

// TODO: Either make it movable, or `enable_shared_from_this`.
struct NX_NETWORK_REST_API Request
{
private:
    mutable UserSession mutableUserSession;

public:
    // TODO: These should be private, all data should be available by getters.
    const UserSession& userSession;
    const nx::network::HostAddress foreignAddress;
    const int serverPort;
    const bool isConnectionSecure = true;

    Request(
        const nx::network::http::Request* httpRequest,
        const UserSession& userSession,
        const nx::network::HostAddress& foreignAddress = nx::network::HostAddress(),
        int serverPort = 0,
        bool isConnectionSecure = true,
        std::optional<Content> content = {});

    struct JsonRpcContext
    {
        nx::json_rpc::Request request;
        std::weak_ptr<nx::json_rpc::WebSocketConnection> connection;
        bool isSubscription = false;
    };
    Request(
        JsonRpcContext jsonRpcContext,
        const UserSession& userSession,
        const nx::network::HostAddress& foreignAddress = nx::network::HostAddress(),
        int serverPort = 0);

    Request(const nx::network::http::Request* httpRequest, std::optional<Content> content);

    /**
     * Method from request, may be overwritten by:
     * - X-Method-Override header.
     * - method_ URL param if ini config allows it.
     */
    const nx::network::http::Method& method() const;

    const std::optional<Content>& content() const { return m_content; }
    const std::optional<JsonRpcContext>& jsonRpcContext() const { return m_jsonRpcContext; }
    void updateContent(QJsonValue value);
    void resetParamsCache() { m_paramsCache.reset(); }
    const nx::network::http::Request* httpRequest() const { return m_httpRequest; };
    const nx::network::http::HttpHeaders& httpHeaders() const { return m_httpHeaders; }
    const nx::utils::Url& url() const { return m_url; }

    QString path() const
    {
        return m_httpRequest
            ? m_httpRequest->requestLine.url.path(QUrl::PrettyDecoded)
            : m_decodedPath;
    }

    QString decodedPath() const;
    void setDecodedPath(const QString& path) { m_decodedPath = path; }

    /**
     * If HTTP method of request may not provide message body, uses URL params.
     * If HTTP method provides message body, parses message body instead (url params may be merged
     * into it if ini_config permits so).
     */
    const Params& params() const;

    std::optional<QString> param(const QString& key) const;

    /** @return parsed param value or std::nullopt on failure. */
    template<typename T>
    std::optional<T> param(const QString& key) const;

    /** @return parsed param value or defaultValue on failure. */
    template<typename T = QString>
    T paramOrDefault(const QString& key, const T& defaultValue = {}) const;

    QString paramOrThrow(const QString& key) const;

    /** @return parsed param value or throws rest::Exception on failure. */
    template<typename T>
    T paramOrThrow(const QString& key) const;

    using NxFusionContent = std::optional<QJsonValue>;
    using NxReflectFields = nx::reflect::DeserializationResult::Fields;
    using ParseInfo = std::variant<NxReflectFields, NxFusionContent>;
    /**
     * Returns a deserialized object from the combination of the url path params, the url query
     * params and the request content body. The body is used in the combination if
     * `nx::network::http::Method::isMessageBodyAllowed` is true. The query params are used if
     * `nx::network::http::Method::isMessageBodyAllowed` is false or if
     * `ini().allowUrlParametersForAnyMethod` is true. Throws Result::Exception if the
     * deserialization of the combination is failed. If parseInfo parameter is not nullptr then it
     * is filled with parsed fields if reflect was used or with the request content for fusion.
     */
    template<typename T>
    T parseContentOrThrow(ParseInfo* parseInfo = nullptr) const
    {
        if constexpr (std::is_same_v<nx::utils::Void, T>)
        {
            if (parseInfo)
            {
                if (useReflect())
                    parseInfo->emplace<NxReflectFields>();
                else
                    parseInfo->emplace<NxFusionContent>();
            }
            return {};
        }
        else
        {
            if (useReflect())
            {
                auto [data, fields] = parseContentByReflectOrThrow<T>();
                if (parseInfo)
                    parseInfo->emplace<NxReflectFields>(std::move(fields));
                return data;
            }
            else
            {
                NxFusionContent* content = nullptr;
                if (parseInfo)
                {
                    parseInfo->emplace<NxFusionContent>();
                    content = &std::get<NxFusionContent>(*parseInfo);
                }
                return parseContentByFusionOrThrow<T>(content);
            }
        }
    }

    /** The safe version of parseContent. */
    template<typename T>
    std::optional<T> parseContent() const;

    template<typename T>
    T parseContentByFusionOrThrow(NxFusionContent* content = nullptr) const;

    /** Prefer to use Handler::prepareAuditRecord() instead. */
    explicit operator audit::Record() const;

    bool isExtraFormattingRequired() const;
    Qn::SerializationFormat responseFormatOrThrow() const;
    std::vector<QString> preferredResponseLocales() const;

    const Params& pathParams() const { return m_pathParams; }
    void setPathParams(Params params)
    {
        m_pathParams = std::move(params);
        m_paramsCache.reset();
    }

    void setConcreteIdProvided(bool value) { m_isConcreteIdProvided = value; }
    bool isConcreteIdProvided() const { return m_isConcreteIdProvided; }

    bool isLocal() const;

    void setApiVersion(size_t version) { m_apiVersion = version; }
    std::optional<size_t> apiVersion() const { return m_apiVersion; }
    bool isApiVersionOlder(size_t version) const
    {
        return !m_apiVersion || *m_apiVersion < version;
    }

    bool useReflect() const { return !isApiVersionOlder(4); }

    class NX_NETWORK_REST_API SystemAccessGuard
    {
    public:
        ~SystemAccessGuard();

    private:
        friend Request;
        SystemAccessGuard(UserAccessData* userAccessData);

    private:
        UserAccessData* m_userAccessData;
        UserAccessData m_origin;
    };
    SystemAccessGuard forceSystemAccess() const;

    void forceSessionAccess(UserAccessData access) const;
    void renameParameter(const QString& oldName, const QString& newName);

private:
    template<typename T, typename = std::void_t<>> struct HasPostprocessFields: std::false_type {};
    template<typename T>
    struct HasPostprocessFields<T, std::void_t<decltype(&T::postprocessFields)>>: std::true_type {};

    nx::network::http::Method calculateMethod() const;
    Params calculateParams() const;

    template<typename T>
    std::tuple<T, NxReflectFields> parseContentByReflectOrThrow() const;

private:
    const nx::network::http::Request* const m_httpRequest = nullptr;
    std::optional<JsonRpcContext> m_jsonRpcContext;
    Params m_urlParams;
    Params m_pathParams;
    nx::network::http::Method m_method;
    std::optional<Content> m_content;
    nx::network::http::HttpHeaders m_httpHeaders;
    nx::utils::Url m_url;
    mutable QString m_decodedPath;
    mutable std::optional<Params> m_paramsCache;
    mutable Qn::SerializationFormat m_responseFormat = Qn::SerializationFormat::unsupported;
    bool m_isConcreteIdProvided = false;
    std::optional<size_t> m_apiVersion;
};

//--------------------------------------------------------------------------------------------------

template<typename T>
std::optional<T> Request::param(const QString& key) const
{
    const auto stringValue = param(key);
    if (!stringValue)
        return std::nullopt;

    T value;
    if (!QnLexical::deserialize(*stringValue, &value))
        return std::nullopt;

    return value;
}

inline std::optional<QString> Request::param(const QString& key) const
{
    return params().findValue(key);
}

template<typename T>
T Request::paramOrDefault(const QString& key, const T& defaultValue) const
{
    auto value = param<T>(key);
    return value ? *value : defaultValue;
}

inline QString Request::paramOrThrow(const QString& key) const
{
    const auto stringValue = param(key);
    if (!stringValue)
        throw Exception::missingParameter(key);

    return *stringValue;
}

template<typename T>
T Request::paramOrThrow(const QString& key) const
{
    const auto stringValue = paramOrThrow(key);
    T value;
    if (!QnLexical::deserialize(stringValue, &value))
        throw Exception::invalidParameter(key, stringValue);

    return value;
}

template<typename T>
std::optional<T> Request::parseContent() const
{
    try
    {
        return parseContentOrThrow<T>();
    }
    catch (const Exception& e)
    {
        NX_VERBOSE(this, "Content parsing error: \"%1\"", e.what());
        return std::nullopt;
    }
}

template<typename T>
T Request::parseContentByFusionOrThrow(NxFusionContent* content) const
{
    T data;
    Params params;
    if (ini().allowUrlParametersForAnyMethod
        || !nx::network::http::Method::isMessageBodyAllowed(method()))
    {
        params.unite(m_urlParams);
    }
    params.unite(m_pathParams);
    QnJsonContext context;
    context.setAllowStringConversions(true);
    const auto deserialize =
        [content, &context, &data](QJsonValue value)
        {
            if (!content)
                context.setStrictMode(true);
            if (!QJson::deserialize(&context, value, &data))
            {
                throw Exception::invalidParameter(
                    context.getFailedKeyValue().first, context.getFailedKeyValue().second);
            }
            if (content && context.areSomeFieldsNotFound())
                content->emplace(std::move(value));
        };
    const bool hasNonRefParameter = params.hasNonRefParameter();
    if (m_content && !m_content->body.isEmpty())
    {
        QJsonObject paramsObject;
        if (hasNonRefParameter)
        {
            paramsObject = params.toJson(/*excludeCommon*/ true);
            if (!QJson::deserialize(&context, paramsObject, &data))
            {
                throw Exception::invalidParameter(
                    context.getFailedKeyValue().first, context.getFailedKeyValue().second);
            }
        }
        if (m_content->type == http::header::ContentType::kForm)
        {
            QUrlQuery urlQuery(
                QUrl::fromEncoded(m_content->body, QUrl::StrictMode).toString());
            auto object = Params::fromUrlQuery(urlQuery).toJson(/*excludeCommon*/ true);
            if (object.isEmpty())
                throw Exception::badRequest("Failed to parse request data");
            if (hasNonRefParameter)
            {
                for (auto it = paramsObject.begin(); it != paramsObject.end(); ++it)
                    object.insert(it.key(), it.value());
            }
            deserialize(std::move(object));
        }
        else if (m_content->type == http::header::ContentType::kJson)
        {
            QJsonValue parsedContent;
            QString error;
            if (!QJsonDetail::deserialize_json(m_content->body, &parsedContent, &error))
                throw Exception::badRequest(error);
            if (parsedContent.isObject() && hasNonRefParameter)
            {
                auto object{parsedContent.toObject()};
                for (auto it = paramsObject.begin(); it != paramsObject.end(); ++it)
                    object.insert(it.key(), it.value());
                parsedContent = object;
            }
            deserialize(std::move(parsedContent));
        }
        else //< TODO: Other content types should go here when supported.
        {
            throw Exception::unsupportedMediaType(NX_FMT("Unsupported type: %1", m_content->type));
        }
    }
    else if (m_jsonRpcContext && m_jsonRpcContext->request.params)
    {
        QJsonValue requestParams{json::serialized(*m_jsonRpcContext->request.params)};
        if (requestParams.isObject() && hasNonRefParameter)
        {
            auto paramsObject{params.toJson(/*excludeCommon*/ true)};
            auto object{requestParams.toObject()};
            for (auto it = paramsObject.begin(); it != paramsObject.end(); ++it)
                object.insert(it.key(), it.value());
            deserialize(std::move(object));
        }
        else
        {
            deserialize(requestParams);
        }
    }
    else
    {
        if (hasNonRefParameter)
            deserialize(params.toJson(/*excludeCommon*/ true));
        else if (method() != nx::network::http::Method::get)
            throw Exception::badRequest("No JSON provided");
    }
    return data;
}

template<typename T>
std::tuple<T, Request::NxReflectFields> Request::parseContentByReflectOrThrow() const
{
    T data;
    NxReflectFields fields;
    Params params;
    if (ini().allowUrlParametersForAnyMethod
        || !nx::network::http::Method::isMessageBodyAllowed(method()))
    {
        params.unite(m_urlParams);
    }
    params.unite(m_pathParams);
    if (!params.isEmpty())
        fields = params.deserializeOrThrow(&data);
    const auto merge =
        [&fields, &data](T* copy, NxReflectFields resultFields)
        {
            using namespace nx::reflect;
            if constexpr ((IsAssociativeContainerV<T> && !IsSetContainerV<T>)
                || (IsUnorderedAssociativeContainerV<T> && !IsUnorderedSetContainerV<T>) )
            {
                nx::reflect::mergeAssociativeContainer(&data, copy, std::move(fields), &resultFields,
                    /*replaceAssociativeContainer*/ true);
            }
            else
            {
                nx::reflect::merge(&data, copy, std::move(fields), &resultFields,
                    /*replaceAssociativeContainer*/ true);
            }
            return resultFields;
        };
    const auto throwIfFailed =
        [](const nx::reflect::DeserializationResult& result)
        {
            if (!result)
            {
                if (result.firstNonDeserializedField)
                {
                    throw Exception::invalidParameter(
                        QString::fromStdString(*result.firstNonDeserializedField),
                        QString::fromStdString(result.firstBadFragment));
                }
                throw Exception::badRequest(QString::fromStdString(result.toString()));
            }
        };
    const auto deserializeWithMerge =
        [&fields, &data, &merge, &throwIfFailed](std::string_view serialized)
        {
            if (fields.empty())
            {
                auto result = nx::reflect::json::deserialize(
                    serialized, &data, nx::reflect::json::DeserializationFlag::fields);
                throwIfFailed(result);
                return std::move(result.fields);
            }

            T copy{data};
            auto result = nx::reflect::json::deserialize(
                serialized, &data, nx::reflect::json::DeserializationFlag::fields);
            throwIfFailed(result);
            if constexpr (HasPostprocessFields<T>::value)
                copy.postprocessFields(&fields);
            return merge(&copy, std::move(result.fields));
        };
    if (m_content && !m_content->body.isEmpty())
    {
        if (m_content->type == nx::network::http::header::ContentType::kForm)
        {
            if (fields.empty())
            {
                auto result = nx::reflect::urlencoded::deserialize(
                    {m_content->body.constData(), (size_t) m_content->body.size()}, &data);
                throwIfFailed(result);
                return {std::move(data), std::move(result.fields)};
            }

            T copy{data};
            auto result = nx::reflect::urlencoded::deserialize(
                {m_content->body.constData(), (size_t) m_content->body.size()}, &data);
            throwIfFailed(result);
            if constexpr (HasPostprocessFields<T>::value)
                copy.postprocessFields(&fields);
            return {std::move(data), merge(&copy, std::move(result.fields))};
        }
        else if (m_content->type == nx::network::http::header::ContentType::kJson)
        {
            return {std::move(data),
                deserializeWithMerge(
                    {m_content->body.constData(), (size_t) m_content->body.size()})};
        }
        else //< TODO: Other content types should go here when supported.
        {
            throw Exception::unsupportedMediaType(NX_FMT("Unsupported type: %1", m_content->type));
        }
    }
    else if (m_jsonRpcContext && m_jsonRpcContext->request.params)
    {
        const nx::reflect::json::DeserializationContext deserializationContext{
            *m_jsonRpcContext->request.params, (int) nx::reflect::json::DeserializationFlag::fields};
        return {std::move(data),
            [&fields, &data, &merge, &throwIfFailed, &deserializationContext]()
            {
                if (fields.empty())
                {
                    auto result = nx::reflect::json::deserialize(deserializationContext, &data);
                    throwIfFailed(result);
                    return std::move(result.fields);
                }

                T copy{data};
                auto result = nx::reflect::json::deserialize(deserializationContext, &data);
                throwIfFailed(result);
                if constexpr (HasPostprocessFields<T>::value)
                    copy.postprocessFields(&fields);
                return merge(&copy, std::move(result.fields));
            }()};
    }
    return {std::move(data), std::move(fields)};
}

} // namespace nx::network::rest
