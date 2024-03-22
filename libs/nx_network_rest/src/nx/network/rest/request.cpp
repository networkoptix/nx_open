// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "request.h"

#include <nx/branding.h>
#include <nx/network/http/custom_headers.h>

#include "nx_network_rest_ini.h"

namespace nx::network::rest {

Request::Request(
    const http::Request* httpRequest,
    const UserSession& userSession,
    const nx::network::HostAddress& foreignAddress,
    int serverPort,
    bool isConnectionSecure,
    bool isJsonRpcRequest)
    :
    httpRequest(httpRequest),
    userSession(userSession),
    foreignAddress(foreignAddress),
    serverPort(serverPort),
    isConnectionSecure(isConnectionSecure),
    isJsonRpcRequest(isJsonRpcRequest),
    m_urlParams(Params::fromUrlQuery(QUrlQuery(httpRequest->requestLine.url.toQUrl()))),
    m_method(calculateMethod())
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
    return m_decodedPath = httpRequest->requestLine.url.path();
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
    const auto acceptHeader = http::getHeaderValue(httpRequest->headers, http::header::kAccept);
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

    if (const auto header = http::getHeaderValue(httpRequest->headers, Qn::kAcceptLanguageHeader);
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
    const auto h = http::getHeaderValue(httpRequest->headers, "X-Method-Override");
    if (!h.empty())
        return h;

    return httpRequest->requestLine.method;
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
    const auto method = httpRequest->requestLine.method;
    Params params;
    params.unite(m_pathParams);
    if (!http::Method::isMessageBodyAllowed(method))
    {
        params.unite(m_urlParams);
        if (!isJsonRpcRequest)
            return params;
    }

    if (ini().allowUrlParametersForAnyMethod)
        params.unite(m_urlParams);

    if (content)
    {
        // TODO: Fix or refactor. This function disregards possible content errors. For example,
        //     invalid JSON content (parsing errors) will be silently ignored.
        //     For details: VMS-41649
        auto json = content->parse();
        if (json && json->type() == QJsonValue::Object)
        {
            auto contentParams = Params::fromJson(json->toObject());
            for (auto [key, value]: m_pathParams.keyValueRange())
            {
                if (const auto duplicate = contentParams.findValue(key))
                {
                    if (isJsonRpcRequest || isEqualParam(value, *duplicate))
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
    }

    return params;
}

QJsonValue Request::calculateContent(bool useException, bool wrapInObject) const
{
    QJsonObject urlParamsContent;
    if (!m_urlParams.isEmpty() || !m_pathParams.isEmpty())
    {
        Params params = m_urlParams;
        params.unite(m_pathParams);
        urlParamsContent = params.toJson(/*excludeCommon*/ true);
    }

    if (!isJsonRpcRequest && !http::Method::isMessageBodyAllowed(httpRequest->requestLine.method))
    {
        if (urlParamsContent.isEmpty())
            return QJsonValue(QJsonValue::Undefined);
        return urlParamsContent;
    }

    QJsonValue parsedContent(QJsonValue::Undefined);
    if (content)
    {
        if (content->type == http::header::ContentType::kForm)
        {
            QUrlQuery urlQuery(QUrl::fromEncoded(content->body, QUrl::StrictMode).toString());
            parsedContent = Params::fromUrlQuery(urlQuery).toJson(/*excludeCommon*/ true);

            if (parsedContent.toObject().isEmpty())
                throw Exception::badRequest("Failed to parse request data");
        }
        else if (content->type == http::header::ContentType::kJson)
        {
            try
            {
                parsedContent = QJson::deserializeOrThrow<QJsonValue>(content->body);
            }
            catch (const QJson::InvalidJsonException& e)
            {
                if (useException)
                    throw Exception::badRequest(e.message());
            }
        }
        else //< TODO: Other content types should go here when supported.
        {
            if (useException)
                throw Exception::unsupportedMediaType(NX_FMT("Unsupported type: %1", content->type));
        }
        if (wrapInObject && !parsedContent.isObject())
            parsedContent = QJsonObject{{"_", parsedContent}};
    }

    if (!ini().allowUrlParametersForAnyMethod)
    {
        NX_DEBUG(this, "Ignoring URL parameters for %1", httpRequest->requestLine);
        if (m_pathParams.isEmpty())
            return parsedContent;
        if (!m_urlParams.isEmpty())
            urlParamsContent = m_pathParams.toJson(/*excludeCommon*/ true);
    }

    if (urlParamsContent.isEmpty())
        return parsedContent;

    if (parsedContent.isUndefined())
        return urlParamsContent;

    if (!parsedContent.isObject())
    {
        NX_DEBUG(
            this, "Unable to merge %1 for %2", parsedContent.type(), httpRequest->requestLine);
        return parsedContent;
    }

    QJsonObject object = parsedContent.toObject();
    for (auto it = urlParamsContent.begin(); it != urlParamsContent.end(); ++it)
        object.insert(it.key(), it.value());
    return object;
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
    return SystemAccessGuard(&const_cast<Request*>(this)->userSession.access);
}

} // namespace nx::network::rest
