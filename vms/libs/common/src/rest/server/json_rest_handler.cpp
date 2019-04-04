#include "json_rest_handler.h"

#include <common/common_module.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/user_resource.h>
#include <nx/utils/string.h>
#include <rest/server/rest_connection_processor.h>
#include <utils/common/util.h>

using namespace nx::network;

rest::Response QnJsonRestHandler::executeGet(const rest::Request& request)
{
    QnJsonRestResult result;
    const auto statusCode = executeGet(request.path(), request.params(), result, request.owner);
    auto response = rest::Response::result(result);
    response.statusCode = static_cast<http::StatusCode::Value>(statusCode);
    return response;
}

rest::Response QnJsonRestHandler::executeDelete(const rest::Request& request)
{
    QnJsonRestResult result;
    const auto statusCode = executeDelete(request.path(), request.params(), result, request.owner);
    auto response = rest::Response::result(result);
    response.statusCode = static_cast<http::StatusCode::Value>(statusCode);
    return response;
}

rest::Response QnJsonRestHandler::executePost(const rest::Request& request)
{
    QnJsonRestResult result;
    const auto statusCode = executePost(
        request.path(), request.params(),
        request.content ? request.content->body : QByteArray(),
        result, request.owner);

    auto response = rest::Response::result(result);
    response.statusCode = static_cast<http::StatusCode::Value>(statusCode);
    return response;
}

rest::Response QnJsonRestHandler::executePut(const rest::Request& request)
{
    QnJsonRestResult result;
    const auto statusCode = executePut(
        request.path(), request.params(),
        request.content ? request.content->body : QByteArray(),
        result, request.owner);

    auto response = rest::Response::result(result);
    response.statusCode = static_cast<http::StatusCode::Value>(statusCode);
    return response;
}

static int setWrongMethodError(
    const QString& path, QnJsonRestResult* result, const QnRestConnectionProcessor* owner)
{
    result->setError(
        QnRestResult::CantProcessRequest,
        lm("Method %1 is not allowed for %2").args(owner->request().requestLine.method, path));

    return static_cast<int>(http::StatusCode::badRequest);
}

int QnJsonRestHandler::executeGet(
    const QString& path, const QnRequestParams& /*params*/, QnJsonRestResult& result,
    const QnRestConnectionProcessor* owner)
{
    return setWrongMethodError(path, &result, owner);
}

int QnJsonRestHandler::executeDelete(
    const QString& path, const QnRequestParams& /*params*/, QnJsonRestResult& result,
    const QnRestConnectionProcessor* owner)
{
    return setWrongMethodError(path, &result, owner);
}

int QnJsonRestHandler::executePost(
    const QString& path, const QnRequestParams& /*params*/, const QByteArray& /*body*/,
    QnJsonRestResult& result, const QnRestConnectionProcessor* owner)
{
    return setWrongMethodError(path, &result, owner);
}

int QnJsonRestHandler::executePut(
    const QString& path, const QnRequestParams& /*params*/, const QByteArray& /*body*/,
    QnJsonRestResult& result, const QnRestConnectionProcessor* owner)
{
    return setWrongMethodError(path, &result, owner);
}

int QnJsonRestHandler::executeGet(
    const QString& /*path*/,
    const QnRequestParamList& /*params*/,
    QByteArray& /*result*/,
    QByteArray& /*contentType*/,
    const QnRestConnectionProcessor* /*owner*/)
{
    NX_ASSERT(false, "Is not supposed to be called");
    return (int) http::StatusCode::notImplemented;
}

int QnJsonRestHandler::executeDelete(
    const QString& /*path*/,
    const QnRequestParamList& /*params*/,
    QByteArray& /*result*/,
    QByteArray& /*contentType*/,
    const QnRestConnectionProcessor* /*owner*/)
{
    NX_ASSERT(false, "Is not supposed to be called");
    return (int) http::StatusCode::notImplemented;
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
    return (int) http::StatusCode::notImplemented;
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
    return (int) http::StatusCode::notImplemented;
}
