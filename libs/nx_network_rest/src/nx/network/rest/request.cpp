// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "request.h"

#include <QtCore/QJsonObject>

#include <nx/branding.h>
#include <nx/utils/json/qjson.h>

#include "audit.h"

namespace nx::network::rest {

namespace {

using Crud = json_rpc::Crud;
using Subs = json_rpc::Subs;

static std::string crudTail(Crud crud)
{
    return '.' + nx::reflect::enumeration::toString(crud);
}

static std::string subsTail(Subs subs)
{
    return '.' + nx::reflect::enumeration::toString(subs);
}

static std::tuple<http::Method, QString> methodAndPath(json_rpc::Context* context)
{
    static const auto kCrudTailToMethod = nx::reflect::enumeration::visitAllItems<Crud>(
        [](auto&&... items)
        {
            static constexpr auto kMethods = std::array{
                /*Crud::all*/ http::Method::get,
                /*Crud::one*/ http::Method::get,
                /*Crud::create*/ http::Method::post,
                /*Crud::set*/ http::Method::put,
                /*Crud::update*/ http::Method::patch,
                /*Crud::delete_*/ http::Method::delete_,

                // 6.0 compatibility.
                /*Crud::get*/ http::Method::get,
                /*Crud::list*/ http::Method::get,
            };

            return std::array{std::make_tuple(
                crudTail(items.value), kMethods.at((size_t) items.value), items.value)...};
        });

    static const auto kSubsTails = nx::reflect::enumeration::visitAllItems<Subs>(
        [](auto&&... e) { return std::array{std::make_pair(subsTail(e.value), e.value)...}; });

    std::string_view method = context->request.method;
    for (const auto& [tail, subs]: kSubsTails)
    {
        if (method.ends_with(tail))
        {
            context->subs = subs;
            method.remove_suffix(tail.size());
            break;
        }
    }

    QString path;
    for (const auto& [crudTail, httpMethod, crud]: kCrudTailToMethod)
    {
        if (method.ends_with(crudTail))
        {
            context->crud = crud;
            path = QString::fromUtf8(method.data(), method.size() - crudTail.size());
            path.replace('.', '/');
            return {httpMethod, path};
        }
    }

    path = QString::fromUtf8(method.data(), method.size());
    path.replace('.', '/');
    return {context->subs ? http::Method::get : http::Method::post, path};
}

std::optional<size_t> parseApiVersion(const QString& path, bool withException = false)
{
    if (path.startsWith("/rest/"))
    {
        auto name = path.split('/')[2];
        if (!name.startsWith('v'))
        {
            if (withException)
                throw Exception::notFound(NX_FMT("API version '%1' is not supported", name));
            return {};
        }

        bool isOk = false;
        const auto number = name.mid(1).toInt(&isOk);
        if (!isOk || number <= 0)
        {
            if (withException)
                throw Exception::notFound(NX_FMT("API version '%1' is not supported", name));
            return {};
        }

        return (size_t) number;
    }

    return {};
}

} // namespace

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
    m_decodedPath = path();
    m_apiVersion = parseApiVersion(m_decodedPath);
}

Request::Request(
    json_rpc::Context jsonRpcContext,
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
    std::tie(m_method, m_decodedPath) = methodAndPath(&m_jsonRpcContext.value());

    if (m_jsonRpcContext->request.method.starts_with("rest."))
    {
        m_decodedPath = '/' + m_decodedPath;
        if (!m_apiVersion)
            m_apiVersion = apiVersionOrThrow(m_decodedPath);
        return;
    }

    // Fill m_content for legacy and deprecated api.
    if (!m_jsonRpcContext->request.params)
    {
        m_content = Content{nx::network::http::header::ContentType::kJson, /*body*/ {}};
        return;
    }

    if (!m_jsonRpcContext->request.params->IsObject())
    {
        m_content = Content{nx::network::http::header::ContentType::kJson,
            QByteArray::fromStdString(
                nx::reflect::json::serialize(*m_jsonRpcContext->request.params))};
        return;
    }

    QJsonValue params{json::serialized(*m_jsonRpcContext->request.params)};
    auto object = params.toObject();
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

const QString Request::kFormatParam("_format");

const http::Method& Request::method() const
{
    return m_method;
}

QString Request::decodedPath() const
{
    return m_decodedPath;
}

void Request::setDecodedPathOrThrow(QString path)
{
    if (path.isEmpty())
        path = this->path();
    if (!m_apiVersion)
        m_apiVersion = apiVersionOrThrow(path);
    m_decodedPath = path;
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
    return param("_local").value_or("false").toLower() != "false";
}

Qn::SerializationFormat Request::responseFormatOrThrow() const
{
    if (m_responseFormat != Qn::SerializationFormat::unsupported)
        return m_responseFormat;

    if (const auto format = param(kFormatParam))
    {
        m_responseFormat = nx::reflect::fromString(
            format->toStdString(), Qn::SerializationFormat::unsupported);
        if (m_responseFormat == Qn::SerializationFormat::unsupported)
            throw Exception::invalidParameter(kFormatParam, *format);
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

    if (const auto header = m_httpRequest
            ? http::getHeaderValue(m_httpRequest->headers, http::header::kAcceptLanguage)
            : std::string{};
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

std::optional<size_t> Request::apiVersionOrThrow(const QString& path)
{
    return parseApiVersion(path, /*withException*/ true);
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
    else if (m_jsonRpcContext
        && m_jsonRpcContext->request.params
        && m_jsonRpcContext->request.params->IsObject())
    {
        json = json::serialized(*m_jsonRpcContext->request.params);
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
    NX_ASSERT(!m_jsonRpcContext);
    m_content->type = http::header::ContentType::kJson;
    m_content->body = QJson::serialized(value);
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
    // TODO: Use nx_reflect here.

    QJsonObject apiInfo{{"method", QString::fromStdString(method().toString())}, {"path", decodedPath()}};
    QJsonValue parsedContent;
    try
    {
        parsedContent = parseContentByFusionOrThrow<QJsonValue>();
    }
    catch (const Exception&)
    {
        return {userSession, apiInfo};
    }

    if (ini().auditOnlyParameterNames)
    {
        auto parameters = parameterNames(parsedContent);
        if (!parameters.isEmpty())
            apiInfo["parameters"] = parameters;
    }
    else
    {
        apiInfo["parameters"] = parsedContent;
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
        && m_jsonRpcContext->request.params->IsObject())
    {
        auto it = m_jsonRpcContext->request.params->FindMember(oldName.toStdString());
        if (NX_ASSERT(it != m_jsonRpcContext->request.params->MemberEnd()))
            it->name.SetString(newName.toStdString(), m_jsonRpcContext->request.allocator);
    }
    if (!m_paramsCache)
        m_paramsCache = calculateParams();
    m_paramsCache->rename(oldName, newName);
}

void Request::removeFieldsFromParams(const NxReflectFields& fields, rapidjson::Value* params)
{
    for (const auto& f: fields)
    {
        auto m = params->FindMember(f.name);
        if (m == params->MemberEnd())
            continue;

        if (m->value.IsObject())
        {
            removeFieldsFromParams(f.fields, &m->value);
            if (m->value.MemberCount() != 0)
                continue;
        }

        params->EraseMember(m);
    }
}

} // namespace nx::network::rest
