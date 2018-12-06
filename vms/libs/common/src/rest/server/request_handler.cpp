#include "request_handler.h"

#include <QtCore/QDebug>

#include <nx/utils/log/log.h>

RestRequest::RestRequest(
    QString path, 
    QnRequestParamList params,
    const QnRestConnectionProcessor* owner,
    const nx::network::http::Request* httpRequest)
:
    path(std::move(path)),
    params(std::move(params)),
    owner(owner),
    httpRequest(httpRequest)
{
}

RestContent::RestContent(QByteArray type, QByteArray body):
    type(std::move(type)), body(std::move(body))

{
}

RestResponse::RestResponse(
    nx::network::http::StatusCode::Value statusCode, RestContent content, bool isUndefinedContentLength)
:
    statusCode(statusCode), content(std::move(content)),
    isUndefinedContentLength(isUndefinedContentLength)
{
}

QnRestRequestHandler::QnRestRequestHandler()
{
}

RestResponse QnRestRequestHandler::executeRequest(
    nx::network::http::Method::ValueType method,
    const RestRequest& request,
    const RestContent& content)
{
    RestResponse response;

    if (method == nx::network::http::Method::get)
    {
        response = executeGet(request);
    }
    else if (method == nx::network::http::Method::post)
    {
        response = executePost(request, content);
    }
    else if (method == nx::network::http::Method::put)
    {
        response = executePut(request, content);
    }
    else if (method == nx::network::http::Method::delete_)
    {
        response = executeDelete(request);
    }
    else
    {
        NX_WARNING(this, lm("Unknown REST method %1").arg(method));
        response.statusCode = nx::network::http::StatusCode::notFound;
        response.content.type = "text/plain";
        response.content.body = "Invalid HTTP method";
    }

    return response;
}

RestResponse QnRestRequestHandler::executeGet(const RestRequest& request)
{
    RestResponse result;
    result.statusCode = static_cast<nx::network::http::StatusCode::Value>(executeGet(
        request.path, request.params, result.content.body, result.content.type, request.owner));

    return result;
}

RestResponse QnRestRequestHandler::executeDelete(const RestRequest& request)
{
    RestResponse result;
    result.statusCode = static_cast<nx::network::http::StatusCode::Value>(executeDelete(
        request.path, request.params, result.content.body, result.content.type, request.owner));

    return result;
}

RestResponse QnRestRequestHandler::executePost(
    const RestRequest& request, const RestContent& content)
{
    RestResponse result;
    result.statusCode = static_cast<nx::network::http::StatusCode::Value>(executePost(
        request.path, request.params, content.body, content.type,
        result.content.body, result.content.type, request.owner));

    return result;
}

RestResponse QnRestRequestHandler::executePut(
    const RestRequest& request, const RestContent& content)
{
    RestResponse result;
    result.statusCode = static_cast<nx::network::http::StatusCode::Value>(executePost(
        request.path, request.params, content.body, content.type,
        result.content.body, result.content.type, request.owner));

    return result;
}

void QnRestRequestHandler::afterExecute(const RestRequest& request, const QByteArray& response)
{
    afterExecute(request.path, request.params, response, request.owner);
}

QString QnRestRequestHandler::extractAction(const QString& path) const
{
    QString localPath = path;
    while(localPath.endsWith(L'/'))
        localPath.chop(1);
    return localPath.mid(localPath.lastIndexOf(L'/') + 1);
}

class QnRestGUIRequestHandlerPrivate
{
public:
    QnRestGUIRequestHandlerPrivate(): result(0), body(0), code(0) {}

    QnRequestParamList params;
    QByteArray* result;
    const QByteArray* body;
    QString path;
    int code;
    QString method;
    QByteArray contentType;
};

QnRestGUIRequestHandler::QnRestGUIRequestHandler(): d_ptr(new QnRestGUIRequestHandlerPrivate)
{
}

QnRestGUIRequestHandler::~QnRestGUIRequestHandler()
{
    delete d_ptr;
}

int QnRestGUIRequestHandler::executeGet(
    const QString& path,
    const QnRequestParamList& params,
    QByteArray& result, QByteArray& contentType,
    const QnRestConnectionProcessor* /*owner*/)
{
    Q_D(QnRestGUIRequestHandler);
    d->path = path;
    d->params = params;
    d->result = &result;
    d->method = QLatin1String("GET");
    d->contentType = contentType;
    QMetaObject::invokeMethod(this, "methodExecutor", Qt::BlockingQueuedConnection);
    return d->code;
}

int QnRestRequestHandler::executeDelete(
    const QString& /*path*/,
    const QnRequestParamList& /*params*/,
    QByteArray& /*result*/,
    QByteArray& /*contentType*/,
    const QnRestConnectionProcessor* /*owner*/)
{
    return nx::network::http::StatusCode::notImplemented;
}

int QnRestGUIRequestHandler::executePost(
    const QString& path,
    const QnRequestParamList& params,
    const QByteArray& body,
    const QByteArray& /*srcBodyContentType*/,
    QByteArray& result,
    QByteArray& contentType,
    const QnRestConnectionProcessor* /*owner*/)
{
    Q_D(QnRestGUIRequestHandler);
    d->params = params;
    d->result = &result;
    d->body = &body;
    d->path = path;
    d->method = QLatin1String("POST");
    d->contentType = contentType;
    QMetaObject::invokeMethod(this, "methodExecutor", Qt::BlockingQueuedConnection);
    return d->code;
}

int QnRestRequestHandler::executePut(
    const QString& path,
    const QnRequestParamList& params,
    const QByteArray& body,
    const QByteArray& srcBodyContentType,
    QByteArray& result,
    QByteArray& resultContentType,
    const QnRestConnectionProcessor* owner)
{
    return executePost(
        path, params, body, srcBodyContentType, result, resultContentType, owner);
}

void QnRestRequestHandler::afterExecute(
    const QString& /*path*/,
    const QnRequestParamList& /*params*/,
    const QByteArray& /*body*/,
    const QnRestConnectionProcessor* /*owner*/)
{
}

void QnRestGUIRequestHandler::methodExecutor()
{
    Q_D(QnRestGUIRequestHandler);
    if (d->method == QLatin1String("GET"))
        d->code = executeGetGUI(d->path, d->params, *d->result);
    else if (d->method == QLatin1String("POST"))
        d->code = executePostGUI(d->path, d->params, *d->body, *d->result);
    else
        qWarning() << "Unknown execute method " << d->method;
}
