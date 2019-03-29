#include "rest_connection_processor.h"

#include <QtCore/QUrl>
#include <QtCore/QUrlQuery>
#include <QtCore/QRegExp>

#include <nx/utils/log/log.h>
#include <nx/utils/gzip/gzip_compressor.h>
#include <nx/utils/gzip/gzip_uncompressor.h>

#include "network/tcp_connection_priv.h"
#include "network/tcp_listener.h"
#include "request_handler.h"
#include "core/resource_management/resource_pool.h"
#include <core/resource_access/resource_access_manager.h>
#include <core/resource/user_resource.h>
#include <core/resource_access/user_access_data.h>
#include <common/common_module.h>
#include <network/http_connection_listener.h>

static const QByteArray NOT_AUTHORIZED_HTML("\
    <!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.01 Transitional//EN\"\"http://www.w3.org/TR/1999/REC-html401-19991224/loose.dtd\">\
    <HTML>\
    <HEAD>\
    <TITLE>Error</TITLE>\
    <META HTTP-EQUIV=\"Content-Type\" CONTENT=\"text/html; charset=utf-8\">\
    </HEAD>\
    <BODY><H1>401 Unauthorized. <br> You don't have required permissions.</H1></BODY>\
    </HTML>"
);

const nx::network::http::Method::ValueType QnRestProcessorPool::kAnyHttpMethod = "";
const QString QnRestProcessorPool::kAnyPath = QString();

void QnRestProcessorPool::registerHandler(
    const QString& path,
    QnRestRequestHandler* handler,
    GlobalPermission permissions)
{
    registerHandler(kAnyHttpMethod, path, handler, permissions);
}

void QnRestProcessorPool::registerHandler(
    nx::network::http::Method::ValueType httpMethod,
    const QString& path,
    QnRestRequestHandler* handler,
    GlobalPermission permissions)
{
    m_handlers[httpMethod].insert(path, QnRestRequestHandlerPtr(handler));
    handler->setPath(path);
    handler->setPermissions(permissions);
}

QnRestRequestHandlerPtr QnRestProcessorPool::findHandler(
    const nx::network::http::Method::ValueType& httpMethod,
    const QString& path) const
{
    if (auto handler = findHandlerForSpecificMethod(httpMethod, path))
        return handler;
    return findHandlerForSpecificMethod(kAnyHttpMethod, path);
}

void QnRestProcessorPool::registerRedirectRule(const QString& path, const QString& newPath)
{
    m_redirectRules.insert(path, newPath);
}

boost::optional<QString> QnRestProcessorPool::getRedirectRule(const QString& path)
{
    const auto it = m_redirectRules.find(path);
    if (it != m_redirectRules.end())
        return it.value();
    else
        return boost::none;
}

QnRestRequestHandlerPtr QnRestProcessorPool::findHandlerForSpecificMethod(
    const nx::network::http::Method::ValueType& httpMethod,
    const QString& path) const
{
    const auto it = m_handlers.find(httpMethod);
    if (it == m_handlers.end())
        return nullptr;

    return findHandlerByPath(it->second, path);
}

QnRestRequestHandlerPtr QnRestProcessorPool::findHandlerByPath(
    const HandlersByPath& handlersByPath,
    const QString& path) const
{
    const auto normalizedPath = QnTcpListener::normalizedPath(path);

    auto it = handlersByPath.upperBound(normalizedPath);
    if (it == handlersByPath.begin())
        return normalizedPath.startsWith(it.key()) ? it.value() : nullptr;
    while (it-- != handlersByPath.begin())
    {
        if (normalizedPath.startsWith(it.key()))
            return it.value();
    }

    if (handlersByPath.contains(kAnyPath))
        return handlersByPath.value(kAnyPath);

    return nullptr;
}

//-------------------------------------------------------------------------------------------------

QnRestConnectionProcessor::QnRestConnectionProcessor(
    std::unique_ptr<nx::network::AbstractStreamSocket> socket,
    QnHttpConnectionListener* owner)
:
    QnTCPConnectionProcessor(std::move(socket), owner),
    m_noAuth(false)
{
}

QnRestConnectionProcessor::~QnRestConnectionProcessor()
{
    stop();
}

void QnRestConnectionProcessor::run()
{
    Q_D(QnTCPConnectionProcessor);

    initSystemThreadId();

    if (d->clientRequest.isEmpty()) {
        if (!readRequest())
            return;
    }
    parseRequest();


    d->response.messageBody.clear();

    RestRequest request(&d->request, this);
    RestResponse response;
    QnRestRequestHandlerPtr handler = static_cast<QnHttpConnectionListener*>(d->owner)
        ->processorPool()->findHandler(request.method(), request.path());
    if (handler)
    {
        if (!m_noAuth && d->accessRights != Qn::kSystemAccess)
        {
            QnUserResourcePtr user = resourcePool()->getResourceById<QnUserResource>(d->accessRights.userId);
            if (!user)
            {
                sendUnauthorizedResponse(nx::network::http::StatusCode::forbidden);
                return;
            }
            if (!resourceAccessManager()->hasGlobalPermission(user, handler->permissions()))
            {
                sendUnauthorizedResponse(nx::network::http::StatusCode::forbidden);
                return;
            }
        }

        const auto requestContentType =
            nx::network::http::getHeaderValue(d->request.headers, "Content-Type");

        if (!requestContentType.isEmpty() || d->requestBody.isEmpty())
            request.content = RestContent{requestContentType, d->requestBody};

        response = handler->executeRequest(request);
    }
    else
    {
        QByteArray type;
        response.statusCode = (nx::network::http::StatusCode::Value) notFound(type);
        response.content = RestContent{type, d->response.messageBody};
    }

    QByteArray contentEncoding;
    if (response.content)
    {
        if (nx::network::http::getHeaderValue(d->request.headers, "Accept-Encoding")
                .toLower().contains("gzip")
            && response.statusCode == nx::network::http::StatusCode::ok
            && !response.content->type.contains("image"))
        {
            contentEncoding = "gzip";
            d->response.messageBody = nx::utils::bstream::gzip::Compressor::compressData(
                response.content->body);
        }
        else
        {
            d->response.messageBody = response.content->body;
        }
    }
    else
    {
        d->response.messageBody = {};
    }

    d->response.headers.insert(
        response.httpHeaders.begin(), response.httpHeaders.end());

    nx::network::http::insertHeader(&d->response.headers,
        {"Cache-Control", "no-store, no-cache, must-revalidate, max-age=0"});
    nx::network::http::insertHeader(&d->response.headers,
        {"Cache-Control", "post-check=0, pre-check=0"});
    nx::network::http::insertHeader(&d->response.headers,
        {"Pragma", "no-cache"});

    sendResponse(
        response.statusCode,
        response.content ? response.content->type : QByteArray(),
        contentEncoding,
        /*multipartBoundary*/ QByteArray(),
        /*displayDebug*/ false,
        response.isUndefinedContentLength);

    if (handler && nx::network::http::StatusCode::isSuccessCode(response.statusCode))
        handler->afterExecute(request, response);
}

Qn::UserAccessData QnRestConnectionProcessor::accessRights() const
{
    Q_D(const QnTCPConnectionProcessor);
    return d->accessRights;
}

void QnRestConnectionProcessor::setAccessRights(const Qn::UserAccessData& accessRights)
{
    Q_D(QnTCPConnectionProcessor);
    d->accessRights = accessRights;
}

void QnRestConnectionProcessor::setAuthNotRequired(bool noAuth)
{
    m_noAuth = noAuth;
}

const nx::network::http::Request& QnRestConnectionProcessor::request() const
{
    Q_D(const QnTCPConnectionProcessor);
    return d->request;
}

nx::network::http::Response* QnRestConnectionProcessor::response() const
{
    Q_D(const QnTCPConnectionProcessor);
    //TODO #ak remove following const_cast in 2.3.1 (requires change in QnRestRequestHandler API)
    return const_cast<nx::network::http::Response*>(&d->response);
}
