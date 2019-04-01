#include "request_handler.h"

#include <QtCore/QDebug>

#include <nx/fusion/serialization/json.h>
#include <nx/network/rest/nx_network_rest_ini.h>
#include <nx/utils/log/log.h>

static const QString kExtraFormatting("extraFormatting");
static const QByteArray kFormContentType("application/x-www-form-urlencoded");
static const QByteArray kJsonContentType("application/json");

std::optional<QJsonValue> RestContent::parse() const
{
    if (type == kFormContentType)
        return RequestParams::fromUrlQuery(QUrlQuery(body)).toJson();

    if (type == kJsonContentType)
    {
        QJsonValue value;
        if (!QJson::deserialize(body, &value))
            return std::nullopt;

        return value;
    }

    // TODO: Other content types should go there when supported.
    return std::nullopt;
}

QString RestException::message() const
{
    return descriptor.text();
}

RestRequest::RestRequest(
    const nx::network::http::Request* httpRequest,
    const QnRestConnectionProcessor* owner)
:
    httpRequest(httpRequest),
    owner(owner),
    m_urlParams(RequestParams::fromUrlQuery(QUrlQuery(httpRequest->requestLine.url.toQUrl()))),
    m_method(calculateMethod().toUpper())
{
}

const nx::network::http::Method::ValueType& RestRequest::method() const
{
    return m_method;
}

QString RestRequest::path() const
{
    return httpRequest->requestLine.url.path();
}

const RequestParams& RestRequest::params() const
{
    if (!m_paramsCache)
        m_paramsCache = calculateParams();

    return *m_paramsCache;
}

bool RestRequest::isExtraFormattingRequired() const
{
    return (bool) param(kExtraFormatting);
}

nx::network::http::Method::ValueType RestRequest::calculateMethod() const
{
    if (nx::network::rest::ini().allowGetMethodReplacement)
    {
        const auto p = m_urlParams.value("method_");
        if (!p.isEmpty())
            return p.toUtf8();
    }

    const auto h = nx::network::http::getHeaderValue(httpRequest->headers, "X-Method-Override");
    if (!h.isEmpty())
        return h;

    return httpRequest->requestLine.method;
}

RequestParams RestRequest::calculateParams() const
{
    const auto method = httpRequest->requestLine.method.toUpper();
    if (!nx::network::http::Method::isMessageBodyAllowed(method))
        return m_urlParams;

    RequestParams params;
    if (nx::network::rest::ini().allowUrlParamitersForAnyMethod)
        params.unite(m_urlParams);

    if (content)
    {
        auto json = content->parse();
        if (json && json->type() == QJsonValue::Object)
            params.unite(RequestParams::fromJson(json->toObject()));
    }

    return params;
}

std::optional<QJsonValue> RestRequest::calculateContent() const
{
    std::optional<QJsonObject> urlParamsContent;
    if (!m_urlParams.isEmpty())
        urlParamsContent = m_urlParams.toJson();

    if (!nx::network::http::Method::isMessageBodyAllowed(httpRequest->requestLine.method))
        return urlParamsContent;

    std::optional<QJsonValue> parsedContent;
    if (content)
        parsedContent = content->parse();

    if (!nx::network::rest::ini().allowUrlParamitersForAnyMethod)
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

RestResponse::RestResponse(nx::network::http::StatusCode::Value statusCode):
    statusCode(statusCode)
{
}

RestResponse RestResponse::result(const QnJsonRestResult& result)
{
    RestResponse response;
    response.statusCode = QnRestResult::toHttpStatus(result.error);
    response.content = RestContent{kJsonContentType, QJson::serialized(result)};
    return response;
}

RestResponse RestResponse::error(const QnRestResult::ErrorDescriptor& descriptor)
{
    QnRestResult rest;
    rest.setError(descriptor);
    return result(rest);
}

RestResponse QnRestRequestHandler::executeRequest(
    const RestRequest& request)
{
    RestResponse response;
    try
    {
        if (request.method() == nx::network::http::Method::get)
            response = executeGet(request);
        else if (request.method() == nx::network::http::Method::post)
            response = executePost(request);
        else if (request.method() == nx::network::http::Method::put)
            response = executePut(request);
        else if (request.method() == nx::network::http::Method::delete_)
            response = executeDelete(request);
        else
            throw RestException(QnRestResult::CantProcessRequest, "Invalid HTTP method");
    }
    catch (const RestException& error)
    {
        return RestResponse::error(error.descriptor);
    }

    if (response.content && response.content->type == kJsonContentType
        && request.isExtraFormattingRequired())
    {
        response.content->body = nx::utils::formatJsonString(response.content->body);
    }

    return response;
}

void QnRestRequestHandler::afterExecute(const RestRequest& request, const RestResponse& response)
{
    afterExecute(
        request.path(), request.params(),
        response.content ? response.content->body : QByteArray(),
        request.owner);
}

RestResponse QnRestRequestHandler::executeGet(const RestRequest& request)
{
    RestResponse result;
    RestContent content;
    result.statusCode = static_cast<nx::network::http::StatusCode::Value>(executeGet(
        request.path(), request.params(),
        content.body, content.type,
        request.owner));

    if (!content.type.isEmpty() || !content.body.isEmpty())
        result.content = std::move(content);

    return result;
}

RestResponse QnRestRequestHandler::executeDelete(const RestRequest& request)
{
    RestResponse result;
    RestContent content;
    result.statusCode = static_cast<nx::network::http::StatusCode::Value>(executeDelete(
        request.path(), request.params(),
        content.body, content.type,
        request.owner));

    if (!content.type.isEmpty() || !content.body.isEmpty())
        result.content = std::move(content);

    return result;
}

RestResponse QnRestRequestHandler::executePost(const RestRequest& request)
{
    RestResponse result;
    RestContent content;
    result.statusCode = static_cast<nx::network::http::StatusCode::Value>(executePost(
        request.path(), request.params(),
        request.content ? request.content->body : QByteArray(),
        request.content ? request.content->type : QByteArray(),
        content.body, content.type,
        request.owner));

    if (!content.type.isEmpty() || !content.body.isEmpty())
        result.content = std::move(content);

    return result;
}

RestResponse QnRestRequestHandler::executePut(const RestRequest& request)
{
    RestResponse result;
    RestContent content;
    result.statusCode = static_cast<nx::network::http::StatusCode::Value>(executePost(
        request.path(), request.params(),
        request.content ? request.content->body : QByteArray(),
        request.content ? request.content->type : QByteArray(),
        content.body, content.type,
        request.owner));

    if (!content.type.isEmpty() || !content.body.isEmpty())
        result.content = std::move(content);

    return result;
}

QString QnRestRequestHandler::extractAction(const QString& path) const
{
    QString localPath = path;
    while(localPath.endsWith(L'/'))
        localPath.chop(1);
    return localPath.mid(localPath.lastIndexOf(L'/') + 1);
}
