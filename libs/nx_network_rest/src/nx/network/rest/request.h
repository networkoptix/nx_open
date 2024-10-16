// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/json_rpc/messages.h>
#include <nx/reflect/json/deserializer.h>
#include <nx/reflect/urlencoded/deserializer.h>
#include <nx/utils/void.h>

#include "audit.h"
#include "exception.h"
#include "nx_network_rest_ini.h"
#include "params.h"

namespace nx::json_rpc { class WebSocketConnection; }

namespace nx::network::rest {

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

    /**
     * Returns a deserialized object from the combination of the url path params, the url query
     * params and the request content body. The body is used in the combination if
     * `nx::network::http::Method::isMessageBodyAllowed` is true. The query params are used if
     * `nx::network::http::Method::isMessageBodyAllowed` is false or if
     * `ini().allowUrlParametersForAnyMethod` is true. Throws Result::Exception if the
     * deserialization of the combination is failed. If some T fields are omitted and content is
     * not nullptr then content is filled with the calculated request content.
     */
    template<typename T = QJsonValue>
    T parseContentOrThrow(std::optional<QJsonValue>* content = nullptr) const;

    using Fields = nx::reflect::DeserializationResult::Fields;
    /**
     * Result is the same as result of parseContentOrThrow. Parsed fields are filled.
     */
    template<typename T>
    T parseContentAllowingOmittedValuesOrThrow(Fields* fields) const;

    /** The safe version of parseContent. */
    template<typename T>
    std::optional<T> parseContent() const;

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
    nx::network::http::Method calculateMethod() const;
    Params calculateParams() const;

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
T Request::parseContentOrThrow(std::optional<QJsonValue>* content) const
{
    T data;
    if constexpr (std::is_same_v<nx::utils::Void, T>)
        return data;

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
        if (m_jsonRpcContext->request.params->isObject() && hasNonRefParameter)
        {
            auto paramsObject{params.toJson(/*excludeCommon*/ true)};
            auto object{m_jsonRpcContext->request.params->toObject()};
            for (auto it = paramsObject.begin(); it != paramsObject.end(); ++it)
                object.insert(it.key(), it.value());
            deserialize(std::move(object));
        }
        else
        {
            deserialize(m_jsonRpcContext->request.params.value());
        }
    }
    else
    {
        if (!hasNonRefParameter)
            throw Exception::badRequest("No JSON provided");
        deserialize(params.toJson(/*excludeCommon*/ true));
    }
    return data;
}

template<typename T>
T Request::parseContentAllowingOmittedValuesOrThrow(Fields* fields) const
{
    T data;
    Params params;
    if (m_content)
    {
        if (m_content->type == nx::network::http::header::ContentType::kForm)
        {
            auto result = nx::reflect::urlencoded::deserialize(
                std::string_view{m_content->body.constData(), (size_t) m_content->body.size()},
                &data);
            if (!result)
                throw Exception::badRequest(QString::fromStdString(result.toString()));
            *fields = std::move(result.fields);
        }
        else if (m_content->type == nx::network::http::header::ContentType::kJson)
        {
            auto result = nx::reflect::json::deserialize(
                std::string_view{m_content->body.constData(), (size_t) m_content->body.size()},
                &data,
                nx::reflect::json::DeserializationFlag::fields);
            if (!result)
                throw Exception::badRequest(QString::fromStdString(result.toString()));
            *fields = std::move(result.fields);
        }
        else //< TODO: Other content types should go here when supported.
        {
            throw Exception::unsupportedMediaType(NX_FMT("Unsupported type: %1", m_content->type));
        }
    }
    else if (m_jsonRpcContext && m_jsonRpcContext->request.params)
    {
        // TODO: Switch JSON RPC to rapidjson.
        auto serialized = QJson::serialize(m_jsonRpcContext->request.params.value());
        auto result = nx::reflect::json::deserialize(
            std::string_view{serialized.constData(), (size_t) serialized.size()},
            &data,
            nx::reflect::json::DeserializationFlag::fields);
        if (!result)
            throw Exception::badRequest(QString::fromStdString(result.toString()));
        *fields = std::move(result.fields);
    }
    params.unite(m_pathParams);
    if (ini().allowUrlParametersForAnyMethod
        || !nx::network::http::Method::isMessageBodyAllowed(method()))
    {
        params.unite(m_urlParams);
    }
    if (!params.isEmpty())
        params.uniteOrThrow(&data, fields);
    return data;
}

} // namespace nx::network::rest
