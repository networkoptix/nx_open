#include "json_rest_handler.h"
#include <utils/common/util.h>

#include <nx/utils/string.h>

namespace {

static const QLatin1String kExtraFormatting("extraFormatting");
static const QByteArray kJsonContetnType("application/json");

} // namespace

JsonRestResponse::JsonRestResponse(
    nx::network::http::StatusCode::Value statusCode, QnJsonRestResult json, bool isUndefinedContentLength)
:
    statusCode(statusCode), json(std::move(json)),
    isUndefinedContentLength(isUndefinedContentLength)
{
}

JsonRestResponse::JsonRestResponse(
    nx_http::StatusCode::Value status, QnJsonRestResult::Error error)
:
    statusCode(statusCode)
{
    json.setError(error);
}

RestResponse JsonRestResponse::toRest(bool extraFormatting) const
{
    RestContent content{kJsonContetnType, QJson::serialized(json)};
    if (extraFormatting)
        content.body = nx::utils::formatJsonString(content.body);

    return {statusCode, std::move(content), isUndefinedContentLength};
}

JsonRestRequest::JsonRestRequest(const RestRequest& rest):
    path(rest.path), params(rest.params.toHash()), owner(rest.owner)
{
}

JsonRestRequest::JsonRestRequest(
    QString path, const QnRequestParams& params, const QnRestConnectionProcessor* owner)
:
    path(std::move(path)), params(params), owner(owner)
{
}

JsonRestResponse QnJsonRestHandler::executeGet(const JsonRestRequest& request)
{
    JsonRestResponse response;
    QByteArray contentType;
    response.statusCode = (nx::network::http::StatusCode::Value) executeGet(
        request.path, request.params, response.json, request.owner);

    return response;
}

JsonRestResponse QnJsonRestHandler::executeDelete(const JsonRestRequest& request)
{
    JsonRestResponse response;
    response.statusCode = (nx::network::http::StatusCode::Value) executeDelete(
        request.path, request.params, response.json, request.owner);

    return response;
}

JsonRestResponse QnJsonRestHandler::executePost(const JsonRestRequest& request, const QByteArray& body)
{
    JsonRestResponse response;
    response.statusCode = (nx::network::http::StatusCode::Value) executePost(
        request.path, request.params, body, response.json, request.owner);

    return response;
}

JsonRestResponse QnJsonRestHandler::executePut(const JsonRestRequest& request, const QByteArray& body)
{
    JsonRestResponse response;
    response.statusCode = (nx::network::http::StatusCode::Value) executePut(
        request.path, request.params, body, response.json, request.owner);

    return response;
}

int QnJsonRestHandler::executeGet(
    const QString& path, const QnRequestParams& params, QnJsonRestResult& result,
    const QnRestConnectionProcessor* owner)
{
    auto response = executePost(JsonRestRequest(path, params, owner), QByteArray());
    result = std::move(response.json);
    return (int) response.statusCode;
}

int QnJsonRestHandler::executeDelete(
    const QString& /*path*/, const QnRequestParams& /*params*/, QnJsonRestResult& /*result*/,
    const QnRestConnectionProcessor* /*owner*/)
{
    return (int) nx::network::http::StatusCode::notImplemented;
}

int QnJsonRestHandler::executePost(
    const QString& path, const QnRequestParams& params, const QByteArray& /*body*/,
    QnJsonRestResult& result, const QnRestConnectionProcessor* owner)
{
    auto response = executeGet(JsonRestRequest(path, params, owner));
    result = std::move(response.json);
    return (int) response.statusCode;
}

int QnJsonRestHandler::executePut(
    const QString& path, const QnRequestParams& params, const QByteArray& body,
    QnJsonRestResult& result, const QnRestConnectionProcessor* owner)
{
    auto response = executePost(JsonRestRequest(path, params, owner), body);
    result = std::move(response.json);
    return (int) response.statusCode;
}

RestResponse QnJsonRestHandler::executeGet(const RestRequest& request)
{
    return executeGet(JsonRestRequest(request))
        .toRest(request.params.contains(kExtraFormatting));
}

RestResponse QnJsonRestHandler::executeDelete(const RestRequest& request)
{
    return executeDelete(JsonRestRequest(request))
        .toRest(request.params.contains(kExtraFormatting));
}

RestResponse QnJsonRestHandler::executePost(const RestRequest& request, const RestContent& content)
{
    return executePost(JsonRestRequest(request), content.body)
        .toRest(request.params.contains(kExtraFormatting));
}

RestResponse QnJsonRestHandler::executePut(const RestRequest& request, const RestContent& content)
{
    return executePut(JsonRestRequest(request), content.body)
        .toRest(request.params.contains(kExtraFormatting));
}

int QnJsonRestHandler::executeGet(
    const QString& /*path*/,
    const QnRequestParamList& /*params*/,
    QByteArray& /*result*/,
    QByteArray& /*contentType*/,
    const QnRestConnectionProcessor* /*owner*/)
{
    NX_ASSERT(false, "Is not supposed to be called");
    return (int) nx::network::http::StatusCode::notImplemented;
}

int QnJsonRestHandler::executeDelete(
    const QString& /*path*/,
    const QnRequestParamList& /*params*/,
    QByteArray& /*result*/,
    QByteArray& /*contentType*/,
    const QnRestConnectionProcessor* /*owner*/)
{
    NX_ASSERT(false, "Is not supposed to be called");
    return (int) nx::network::http::StatusCode::notImplemented;
}

int QnJsonRestHandler::executePost(
    const QString& /*path*/,
    const QnRequestParamList& /*params*/,
    const QByteArray& /*body*/,
    const QByteArray& /*srcBodyContentType*/,
    QByteArray& /*result*/,
    QByteArray& /*contentType*/,
    const QnRestConnectionProcessor* /*owner*/)
{
    NX_ASSERT(false, "Is not supposed to be called");
    return (int) nx::network::http::StatusCode::notImplemented;
}

int QnJsonRestHandler::executePut(
    const QString& /*path*/,
    const QnRequestParamList& /*params*/,
    const QByteArray& /*body*/,
    const QByteArray& /*srcBodyContentType*/,
    QByteArray& /*result*/,
    QByteArray& /*contentType*/,
    const QnRestConnectionProcessor* /*owner*/)
{
    NX_ASSERT(false, "Is not supposed to be called");
    return (int) nx::network::http::StatusCode::notImplemented;
}

QnRequestParams QnJsonRestHandler::processParams(const QnRequestParamList& params) const
{
    QnRequestParams result;
    for (const QnRequestParam& param: params)
        result.insertMulti(param.first, param.second);
    return result;
}
