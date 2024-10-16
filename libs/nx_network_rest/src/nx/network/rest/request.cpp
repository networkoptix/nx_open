// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "request.h"

#include <QtCore/QJsonObject>

#include <nx/branding.h>
#include <nx/network/http/custom_headers.h>

#include "nx_network_rest_ini.h"

namespace nx::network::rest {

static std::tuple<nx::network::http::Method, QString> methodAndPath(const std::string& method)
{
    static constexpr std::array<std::pair<std::string_view, std::string_view>, 7>
        kValueTailToMethod = {
            std::make_pair(std::string_view(".create"), nx::network::http::Method::post),
            std::make_pair(std::string_view(".list"), nx::network::http::Method::get),
            std::make_pair(std::string_view(".get"), nx::network::http::Method::get),
            std::make_pair(std::string_view(".update"), nx::network::http::Method::patch),
            std::make_pair(std::string_view(".delete"), nx::network::http::Method::delete_),
            std::make_pair(std::string_view(".subscribe"), nx::network::http::Method::get),
            std::make_pair(std::string_view(".unsubscribe"), nx::network::http::Method::get),
        };

    QString path;
    for (const auto& [valueTail, httpMethod]: kValueTailToMethod)
    {
        if (method.ends_with(valueTail))
        {
            path = QString::fromLatin1(method.data(), method.size() - valueTail.size());
            path.replace('.', '/');
            return {httpMethod, path};
        }
    }

    path = QString::fromStdString(method);
    path.replace('.', '/');
    return {nx::network::http::Method::post, path};
}

Request::Request(
    const http::Request* httpRequest,
    const UserSession& userSession,
    const nx::network::HostAddress& foreignAddress,
    int serverPort,
    bool isConnectionSecure,
    std::optional<Content> content)
    :
    mutableUserSession(userSession),
    userSession(mutableUserSession),
    foreignAddress(foreignAddress),
    serverPort(serverPort),
    isConnectionSecure(isConnectionSecure),
    m_httpRequest(httpRequest),
    m_urlParams(Params::fromUrlQuery(QUrlQuery(httpRequest->requestLine.url.toQUrl()))),
    m_method(calculateMethod()),
    m_content(std::move(content)),
    m_httpHeaders(httpRequest->headers),
    m_url(httpRequest->requestLine.url)
{
}

Request::Request(
    JsonRpcContext jsonRpcContext,
    const UserSession& userSession,
    const nx::network::HostAddress& foreignAddress,
    int serverPort)
    :
    mutableUserSession(userSession),
    userSession(mutableUserSession),
    foreignAddress(foreignAddress),
    serverPort(serverPort),
    m_jsonRpcContext(std::move(jsonRpcContext)),
    m_responseFormat(Qn::SerializationFormat::json)
{
    std::tie(m_method, m_decodedPath) = methodAndPath(m_jsonRpcContext->request.method);

    if (m_jsonRpcContext->request.method.starts_with("rest."))
        return;

    // Fill m_content for legacy and deprecated api.
    if (!m_jsonRpcContext->request.params)
    {
        m_content = Content{nx::network::http::header::ContentType::kJson, /*body*/ {}};
        return;
    }

    if (!m_jsonRpcContext->request.params->isObject())
    {
        m_content = Content{nx::network::http::header::ContentType::kJson,
            QJson::serialized(m_jsonRpcContext->request.params.value())};
        return;
    }

    auto object = m_jsonRpcContext->request.params->toObject();
    QJsonObject body;
    for (auto it = object.begin(); it != object.end(); ++it)
    {
        if (!it.key().startsWith("_"))
            body.insert(it.key(), it.value());
    }
    m_content = Content{nx::network::http::header::ContentType::kJson,
        body.isEmpty() ? QByteArray{} : QJson::serialized(body)};
}

Request::Request(const nx::network::http::Request* httpRequest, std::optional<Content> content):
    Request(
        httpRequest,
        kSystemSession,
        HostAddress{},
        /*serverPort*/ 0,
        /*isConnectionSecure*/ true,
        std::move(content))
{
}

const http::Method& Request::method() const
{
    return m_method;
}

QString Request::decodedPath() const
{
    if (!m_decodedPath.isEmpty())
        return m_decodedPath;
    return m_decodedPath = path();
}

const Params& Request::params() const
{
    if (!m_paramsCache)
        m_paramsCache = calculateParams();

    return *m_paramsCache;
}

bool Request::isExtraFormattingRequired() const
{
    static const QString kPretty("_pretty");
    static const QString kExtraFormatting("extraFormatting");
    return (bool) param(m_apiVersion.has_value() ? kPretty : kExtraFormatting);
}

bool Request::isLocal() const
{
    static const QString kLocal("_local");
    return (bool) param(kLocal);
}

Qn::SerializationFormat Request::responseFormatOrThrow() const
{
    if (m_responseFormat != Qn::SerializationFormat::unsupported)
        return m_responseFormat;

    static const QString kFormat("_format");
    if (const auto format = param(kFormat))
    {
        m_responseFormat = nx::reflect::fromString(
            format->toStdString(), Qn::SerializationFormat::unsupported);
        if (m_responseFormat == Qn::SerializationFormat::unsupported)
            throw Exception::invalidParameter(kFormat, *format);
        return m_responseFormat;
    }

    static const QString kBackwardCompatibilityFormat("format");
    if (const auto format = param(kBackwardCompatibilityFormat))
    {
        return m_responseFormat =
            nx::reflect::fromString(format->toStdString(), Qn::SerializationFormat::json);
    }
    const auto acceptHeader = http::getHeaderValue(m_httpRequest->headers, http::header::kAccept);
    if (acceptHeader.empty())
        return m_responseFormat = Qn::SerializationFormat::json;

    const auto format = Qn::serializationFormatFromHttpContentType(acceptHeader);
    return m_responseFormat = (format != Qn::SerializationFormat::unsupported)
        ? format
        : Qn::SerializationFormat::json;
}

std::vector<QString> Request::preferredResponseLocales() const
{
    std::vector<QString> locales;
    if (const auto p = m_urlParams.value("_language"); !p.isEmpty())
        locales.push_back(p);

    if (const auto header = http::getHeaderValue(m_httpRequest->headers, Qn::kAcceptLanguageHeader);
        !header.empty())
    {
        nx::utils::split(
            header, ',',
            [&locales](const auto& item)
            {
                const auto [tokens, count] = nx::utils::split_n<2>(item, ';'); //< Drop extra params.
                auto locale = nx::utils::trim(tokens.at(0)); //< Drop extra params.
                // HTTP suggests "en-US", while we expect "en_US".
                locales.push_back(QString::fromStdString(nx::utils::replace(locale, "-", "_")));
            });
    }

    // Use default locale if header is missing.
    locales.push_back(nx::branding::defaultLocale());
    return locales;
}

http::Method Request::calculateMethod() const
{
    const auto h = http::getHeaderValue(m_httpRequest->headers, "X-Method-Override");
    if (!h.empty())
        return h;

    return m_httpRequest->requestLine.method;
}

static bool isEqualParam(const QString& original, const QString& duplicate)
{
    if (!nx::Uuid::isUuidString(original) || !nx::Uuid::isUuidString(duplicate))
        return original == duplicate;

    const nx::Uuid duplicateId(duplicate);
    if (duplicateId.isNull())
        return true;

    return duplicateId == nx::Uuid(original);
}

Params Request::calculateParams() const
{
    Params params;
    params.unite(m_pathParams);
    if (!http::Method::isMessageBodyAllowed(method()))
    {
        params.unite(m_urlParams);
        if (!jsonRpcContext())
            return params;
    }

    if (ini().allowUrlParametersForAnyMethod)
        params.unite(m_urlParams);

    std::optional<QJsonValue> json;
    if (m_content)
    {
        // TODO: Fix or refactor. This function disregards possible content errors. For example,
        //     invalid JSON content (parsing errors) will be silently ignored.
        //     For details: VMS-41649
        json = m_content->parse();
    }
    else if (m_jsonRpcContext)
    {
        json = m_jsonRpcContext->request.params;
    }

    if (json && json->type() == QJsonValue::Object)
    {
        auto contentParams = Params::fromJson(json->toObject());
        for (auto [key, value]: m_pathParams.keyValueRange())
        {
            if (const auto duplicate = contentParams.findValue(key))
            {
                if (jsonRpcContext() || isEqualParam(value, *duplicate))
                {
                    contentParams.remove(key);
                }
                else
                {
                    throw Exception::badRequest(NX_FMT(
                        "Parameter '%1' path value '%2' doesn't match content value '%3'",
                        key, value, *duplicate));
                }
            }
        }
        params.unite(contentParams);
    }
    return params;
}

void Request::updateContent(QJsonValue value)
{
    if (m_jsonRpcContext)
    {
        m_jsonRpcContext->request.params = value;
    }
    else
    {
        m_content->type = http::header::ContentType::kJson;
        m_content->body = QJson::serialized(value);
    }
    m_paramsCache.reset();
}

namespace {

QJsonArray parameterNames(const QJsonValueConstRef& value);

QJsonArray parameterNames(const QJsonObject& object)
{
    QJsonArray result;
    for (auto it = object.begin(); it != object.end(); ++it)
    {
        auto r = parameterNames(it.value());
        if (r.isEmpty())
            result.append(it.key());
        else
            result.append(QJsonObject{{it.key(), r}});
    }
    return result;
}

QJsonArray parameterNames(const QJsonArray& list)
{
    QJsonArray result;

    // Assume that list value types are the same.
    if (list.isEmpty() || !list.first().isObject())
        return result;

    for (const auto& item: list)
    {
        NX_ASSERT(item.isObject());
        result.append(parameterNames(item.toObject()));
    }
    return result;
}

QJsonArray parameterNames(const QJsonValueConstRef& value)
{
    if (value.isArray())
        return parameterNames(value.toArray());

    if (value.isObject())
        return parameterNames(value.toObject());

    return {};
}

QJsonArray parameterNames(const QJsonValue& value)
{
    if (value.isArray())
        return parameterNames(value.toArray());

    if (value.isObject())
        return parameterNames(value.toObject());

    return {};
}

} // namespace

Request::operator audit::Record() const
{
    QJsonObject apiInfo{{"method", QString::fromStdString(method().toString())}, {"path", decodedPath()}};
    auto parsedContent = parseContent<QJsonValue>();
    if (!parsedContent)
        return {userSession, apiInfo};

    if (ini().auditOnlyParameterNames)
    {
        auto parameters = parameterNames(*parsedContent);
        if (!parameters.isEmpty())
            apiInfo["parameters"] = parameters;
    }
    else
    {
        apiInfo["parameters"] = *parsedContent;
    }
    return {userSession, apiInfo};
}

Request::SystemAccessGuard::SystemAccessGuard(UserAccessData* userAccessData):
    m_userAccessData(userAccessData), m_origin(std::move(*userAccessData))
{
    *userAccessData = kSystemAccess;
}

Request::SystemAccessGuard::~SystemAccessGuard()
{
    *m_userAccessData = std::move(m_origin);
}

Request::SystemAccessGuard Request::forceSystemAccess() const
{
    return SystemAccessGuard(&mutableUserSession.access);
}

void Request::forceSessionAccess(UserAccessData access) const
{
    mutableUserSession.access = std::move(access);
}

void Request::renameParameter(const QString& oldName, const QString& newName)
{
    m_urlParams.rename(oldName, newName);
    m_pathParams.rename(oldName, newName);
    if (m_jsonRpcContext
        && m_jsonRpcContext->request.params
        && m_jsonRpcContext->request.params->isObject())
    {
        auto object = m_jsonRpcContext->request.params->toObject();
        auto value = object.take(oldName);
        if (!value.isUndefined())
        {
            object.insert(newName, value);
            m_jsonRpcContext->request.params = object;
        }
    }
    if (!m_paramsCache)
        m_paramsCache = calculateParams();
    m_paramsCache->rename(oldName, newName);
}

} // namespace nx::network::rest
