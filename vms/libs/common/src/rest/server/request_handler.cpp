#include "request_handler.h"

using namespace nx::network;

rest::Response QnRestRequestHandler::executeGet(const rest::Request& request)
{
    rest::Response result;
    rest::Content content;
    result.statusCode = static_cast<http::StatusCode::Value>(executeGet(
        request.path(), request.params(),
        content.body, content.type,
        request.owner));

    if (!content.type.isEmpty() || !content.body.isEmpty())
        result.content = std::move(content);

    return result;
}

rest::Response QnRestRequestHandler::executeDelete(const rest::Request& request)
{
    rest::Response result;
    rest::Content content;
    result.statusCode = static_cast<http::StatusCode::Value>(executeDelete(
        request.path(), request.params(),
        content.body, content.type,
        request.owner));

    if (!content.type.isEmpty() || !content.body.isEmpty())
        result.content = std::move(content);

    return result;
}

rest::Response QnRestRequestHandler::executePost(const rest::Request& request)
{
    rest::Response result;
    rest::Content content;
    result.statusCode = static_cast<http::StatusCode::Value>(executePost(
        request.path(), request.params(),
        request.content ? request.content->body : QByteArray(),
        request.content ? request.content->type : QByteArray(),
        content.body, content.type,
        request.owner));

    if (!content.type.isEmpty() || !content.body.isEmpty())
        result.content = std::move(content);

    return result;
}

rest::Response QnRestRequestHandler::executePut(const rest::Request& request)
{
    rest::Response result;
    rest::Content content;
    result.statusCode = static_cast<http::StatusCode::Value>(executePost(
        request.path(), request.params(),
        request.content ? request.content->body : QByteArray(),
        request.content ? request.content->type : QByteArray(),
        content.body, content.type,
        request.owner));

    if (!content.type.isEmpty() || !content.body.isEmpty())
        result.content = std::move(content);

    return result;
}

int QnRestRequestHandler::executeGet(
    const QString& /*path*/,
    const QnRequestParamList& /*params*/,
    QByteArray& /*result*/,
    QByteArray& /*contentType*/,
    const QnRestConnectionProcessor* /*owner*/)
{
    return http::StatusCode::notImplemented;
}

int QnRestRequestHandler::executeDelete(
    const QString& /*path*/,
    const QnRequestParamList& /*params*/,
    QByteArray& /*result*/,
    QByteArray& /*contentType*/,
    const QnRestConnectionProcessor* /*owner*/)
{
    return http::StatusCode::notImplemented;
}

int QnRestRequestHandler::executePost(
    const QString& /*path*/,
    const QnRequestParamList& /*params*/,
    const QByteArray& /*body*/,
    const QByteArray& /*srcBodyContentType*/,
    QByteArray& /*result*/,
    QByteArray& /*resultContentType*/,
    const QnRestConnectionProcessor* /*owner*/)
{
    return http::StatusCode::notImplemented;
}

int QnRestRequestHandler::executePut(
    const QString& /*path*/,
    const QnRequestParamList& /*params*/,
    const QByteArray& /*body*/,
    const QByteArray& /*srcBodyContentType*/,
    QByteArray& /*result*/,
    QByteArray& /*resultContentType*/,
    const QnRestConnectionProcessor* /*owner*/)
{
    return http::StatusCode::notImplemented;
}

void QnRestRequestHandler::afterExecute(
    const QString& /*path*/,
    const QnRequestParamList& /*params*/,
    const QByteArray& /*body*/,
    const QnRestConnectionProcessor* /*owner*/)
{
}
