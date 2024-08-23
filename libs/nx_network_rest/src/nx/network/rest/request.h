// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/json_rpc/messages.h>

#include "audit.h"
#include "exception.h"
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
     * deserialization of the combination is failed.
     * @param wrapInObject causes non-object payload to be wrapped into object `{"_": payload}` so
     *     other parameter sources are still accessible.
     */
    template<typename T = QJsonValue>
    T parseContentOrThrow(bool wrapInObject = false) const;

    /**
     * Result is the same as result of parseContentOrThrow. If some T fields are omitted then
     * content is filled with the calculated request content.
     */
    template<typename T>
    T parseContentAllowingOmittedValuesOrThrow(
        std::optional<QJsonValue>* content, bool wrapInObject = false) const;

    /** The safe version of parseContent. */
    template<typename T>
    std::optional<T> parseContent(bool wrapInObject = false) const;

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
    [[nodiscard]] SystemAccessGuard forceSystemAccess() const;

    void forceSessionAccess(UserAccessData access) const;
    void renameParameter(const QString& oldName, const QString& newName);

private:
    nx::network::http::Method calculateMethod() const;
    Params calculateParams() const;
    QJsonValue calculateContent(bool useException, bool wrapInObject) const;

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
std::optional<T> Request::parseContent(bool wrapInObject) const
{
    const auto value = calculateContent(/*useException*/ false, wrapInObject);
    if (value.isUndefined())
        return std::nullopt;

    T result;
    QnJsonContext context;
    context.setAllowStringConversions(true);
    if (!QJson::deserialize(&context, value, &result))
        return std::nullopt;

    return result;
}

template<typename T>
T Request::parseContentOrThrow(bool wrapInObject) const
{
    try
    {
        return QJson::deserializeOrThrow<T>(
            calculateContent(/*useException*/ true, wrapInObject),
            /*allowStringConversions*/ true);
    }
    catch (const QJson::InvalidParameterException& e)
    {
        throw Exception::invalidParameter(e.param(), e.value());
    }
    catch (const QJson::InvalidJsonException& e)
    {
        throw Exception::badRequest(e.what());
    }
}

template<typename T>
T Request::parseContentAllowingOmittedValuesOrThrow(
    std::optional<QJsonValue>* content, bool wrapInObject) const
{
    try
    {
        QJsonValue calculatedContent = calculateContent(/*useException*/ true, wrapInObject);
        if (calculatedContent.isUndefined())
            throw Exception::badRequest("No JSON provided.");
        QnJsonContext ctx;
        ctx.setAllowStringConversions(true);
        T result = QJson::deserializeOrThrow<T>(&ctx, calculatedContent);
        if (ctx.areSomeFieldsNotFound())
            *content = std::move(calculatedContent);
        return result;
    }
    catch (const QJson::InvalidParameterException& e)
    {
        throw Exception::invalidParameter(e.param(), e.value());
    }
    catch (const QJson::InvalidJsonException& e)
    {
        throw Exception::badRequest(e.what());
    }
}

} // namespace nx::network::rest
