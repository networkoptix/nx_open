#include "request.h"

#include <nx/network/rest/nx_network_rest_ini.h>

namespace nx::network::rest {

Request::Request(
    const http::Request* httpRequest,
    const QnRestConnectionProcessor* owner)
    :
    httpRequest(httpRequest),
    owner(owner),
    m_urlParams(Params::fromUrlQuery(QUrlQuery(httpRequest->requestLine.url.toQUrl()))),
    m_method(calculateMethod().toUpper())
{
}

const http::Method::ValueType& Request::method() const
{
    return m_method;
}

QString Request::path() const
{
    return httpRequest->requestLine.url.path();
}

const Params& Request::params() const
{
    if (!m_paramsCache)
        m_paramsCache = calculateParams();

    return *m_paramsCache;
}

bool Request::isExtraFormattingRequired() const
{
    static const QString kExtraFormatting("extraFormatting");
    return (bool) param(kExtraFormatting);
}

Qn::SerializationFormat Request::expectedResponseFormat() const
{
    static const QString kFormat("format");
    if (const auto format = param(kFormat))
        return QnLexical::deserialized<Qn::SerializationFormat>(*format, Qn::JsonFormat);

    // TODO: Look into "Accept" header as well.
    return Qn::JsonFormat;
}

http::Method::ValueType Request::calculateMethod() const
{
    if (ini().allowGetMethodReplacement)
    {
        const auto p = m_urlParams.value("method_");
        if (!p.isEmpty())
            return p.toUtf8();
    }

    const auto h = http::getHeaderValue(httpRequest->headers, "X-Method-Override");
    if (!h.isEmpty())
        return h;

    return httpRequest->requestLine.method;
}

Params Request::calculateParams() const
{
    const auto method = httpRequest->requestLine.method.toUpper();
    if (!http::Method::isMessageBodyAllowed(method))
        return m_urlParams;

    Params params;
    if (ini().allowUrlParametersForAnyMethod)
        params.unite(m_urlParams);

    if (content)
    {
        auto json = content->parse();
        if (json && json->type() == QJsonValue::Object)
            params.unite(Params::fromJson(json->toObject()));
    }

    return params;
}

std::optional<QJsonValue> Request::calculateContent() const
{
    std::optional<QJsonObject> urlParamsContent;
    if (!m_urlParams.isEmpty())
        urlParamsContent = m_urlParams.toJson();

    if (!http::Method::isMessageBodyAllowed(httpRequest->requestLine.method))
        return urlParamsContent;

    std::optional<QJsonValue> parsedContent;
    if (content)
        parsedContent = content->parse();

    if (!ini().allowUrlParametersForAnyMethod)
    {
        NX_DEBUG(this, "Ignoring URL parameters for %1", httpRequest->requestLine);
        return parsedContent;
    }

    if (!urlParamsContent)
        return parsedContent;

    if (!parsedContent)
        return urlParamsContent;

    if (parsedContent && parsedContent->type() != QJsonValue::Object)
    {
        NX_DEBUG(this, "Unable to merge %1 for %2",
            parsedContent->type(), httpRequest->requestLine);
        return parsedContent;
    }

    QJsonObject object;
    if (parsedContent)
        object = parsedContent->toObject();

    for (auto it = urlParamsContent->begin(); it != urlParamsContent->end(); ++it)
        object.insert(it.key(), it.value());

    return object;
}

} // namespace nx::network::rest
