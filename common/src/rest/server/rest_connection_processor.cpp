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

QnRestProcessorPool::QnRestProcessorPool()
{
}

void QnRestProcessorPool::registerHandler(const QString& path, QnRestRequestHandler* handler, Qn::GlobalPermission permissions )
{
    m_handlers.insert(path, QnRestRequestHandlerPtr(handler));
    handler->setPath(path);
    handler->setPermissions(permissions);

}

QnRestRequestHandlerPtr QnRestProcessorPool::findHandler( QString path ) const
{
    path = QnTcpListener::normalizedPath(path);


    Handlers::const_iterator i = m_handlers.upperBound(path);
    if (i == m_handlers.begin())
        return path.startsWith(i.key()) ? i.value() : QnRestRequestHandlerPtr();
    while (i-- != m_handlers.begin())
    {
        if (path.startsWith(i.key()))
            return i.value();
    }

    return QnRestRequestHandlerPtr();
}

const QnRestProcessorPool::Handlers& QnRestProcessorPool::handlers() const
{
    return m_handlers;
}

void QnRestProcessorPool::registerRedirectRule( const QString& path, const QString& newPath )
{
    m_redirectRules.insert( path, newPath );
}

boost::optional<QString> QnRestProcessorPool::getRedirectRule( const QString& path )
{
    const auto it = m_redirectRules.find( path );
    if (it != m_redirectRules.end())
        return it.value();
    else
        return boost::none;
}

QnRestConnectionProcessor::QnRestConnectionProcessor(
    QSharedPointer<nx::network::AbstractStreamSocket> socket,
    QnHttpConnectionListener* owner)
:
    QnTCPConnectionProcessor(socket, owner),
    m_noAuth(false)
{
}

QnRestConnectionProcessor::~QnRestConnectionProcessor()
{
    stop();
}

QnTcpListener* QnRestConnectionProcessor::owner() const
{
    Q_D(const QnTCPConnectionProcessor);
    return d->owner;
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

    nx::utils::Url url = getDecodedUrl();
    RestRequest request{url.path(),  QUrlQuery(url.query()).queryItems(QUrl::FullyDecoded), this};
    RestResponse response;

    QnRestRequestHandlerPtr handler = static_cast<QnHttpConnectionListener*>(d->owner)
        ->processorPool()->findHandler(request.path);
    if (handler)
    {
        if (!m_noAuth && d->accessRights != Qn::kSystemAccess)
        {
            QnUserResourcePtr user = resourcePool()->getResourceById<QnUserResource>(d->accessRights.userId);
            if (!user)
            {
                sendUnauthorizedResponse(nx::network::http::StatusCode::forbidden, NOT_AUTHORIZED_HTML);
                return;
            }
            if (!resourceAccessManager()->hasGlobalPermission(user, handler->permissions()))
            {
                sendUnauthorizedResponse(nx::network::http::StatusCode::forbidden, NOT_AUTHORIZED_HTML);
                return;
            }
        }

        const auto requestContentType = nx::network::http::getHeaderValue(d->request.headers, "Content-Type");
        const auto method = d->request.requestLine.method.toUpper();
        if (method == nx::network::http::Method::get)
        {
            response = handler->executeGet(request);
        }
        else
        if (method == nx::network::http::Method::post)
        {
            response = handler->executePost(request, {requestContentType, d->requestBody});
        }
        else
        if (method == nx::network::http::Method::put)
        {
            response = handler->executePut(request, {requestContentType, d->requestBody});
        }
        else
        if (method == nx::network::http::Method::delete_)
        {
            response = handler->executeDelete(request);
        }
        else
        {
            NX_WARNING(this, lm("Unknown REST method %1").arg(method));
            response.statusCode = nx::network::http::StatusCode::notFound;
            response.content.type = "text/plain";
            response.content.body = "Invalid HTTP method";
        }
    }
    else
    {
        response.statusCode = (nx::network::http::StatusCode::Value)
            redirectTo(QnTcpListener::defaultPage(), response.content.type);
    }

    QByteArray contentEncoding;
    if (nx::network::http::getHeaderValue(d->request.headers, "Accept-Encoding").toLower().contains("gzip")
        && !response.content.body.isEmpty() && response.statusCode == nx::network::http::StatusCode::ok
        && !response.content.type.contains("image"))
    {
            d->response.messageBody = nx::utils::bstream::gzip::Compressor::compressData(response.content.body);
            contentEncoding = "gzip";
    }
    else
    {
        d->response.messageBody = response.content.body;
    }

    nx::network::http::insertHeader(&d->response.headers, nx::network::http::HttpHeader("Cache-Control", "no-store, no-cache, must-revalidate, max-age=0"));
    nx::network::http::insertHeader(&d->response.headers, nx::network::http::HttpHeader("Cache-Control", "post-check=0, pre-check=0"));
    nx::network::http::insertHeader(&d->response.headers, nx::network::http::HttpHeader("Pragma", "no-cache"));

    sendResponse(response.statusCode, response.content.type, contentEncoding,
        /*multipartBoundary*/ QByteArray(), /*displayDebug*/ false,
        response.isUndefinedContentLength);

    if (handler)
        handler->afterExecute(request, response.content.body);
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
