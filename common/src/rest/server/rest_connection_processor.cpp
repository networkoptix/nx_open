
#include <QtCore/QUrl>
#include <QtCore/QUrlQuery>
#include <QtCore/QRegExp>

#include "rest_connection_processor.h"
#include "network/tcp_connection_priv.h"
#include "network/tcp_listener.h"
#include "request_handler.h"
#include "utils/gzip/gzip_compressor.h"
#include "core/resource_management/resource_pool.h"
#include <core/resource_management/resource_access_manager.h>
#include <core/resource/user_resource.h>

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



class QnRestConnectionProcessorPrivate: public QnTCPConnectionProcessorPrivate
{
public:
    QnTcpListener* owner;
};

QnRestConnectionProcessor::QnRestConnectionProcessor(QSharedPointer<AbstractStreamSocket> socket, QnTcpListener* _owner):
    QnTCPConnectionProcessor(new QnRestConnectionProcessorPrivate, socket),
    m_noAuth(false)
{
    Q_D(QnRestConnectionProcessor);
    d->owner = _owner;
}

QnRestConnectionProcessor::~QnRestConnectionProcessor()
{
    stop();
}

QnTcpListener* QnRestConnectionProcessor::owner() const
{
    Q_D(const QnRestConnectionProcessor);
    return d->owner;
}

void QnRestConnectionProcessor::run()
{
    Q_D(QnRestConnectionProcessor);

    initSystemThreadId();

    if (d->clientRequest.isEmpty()) {
        if (!readRequest())
            return;
    }
    parseRequest();


    d->response.messageBody.clear();

    QUrl url = getDecodedUrl();
    QString path = url.path();
    QList<QPair<QString, QString> > params = QUrlQuery(url.query()).queryItems(QUrl::FullyDecoded);
    int rez = CODE_OK;
    QByteArray contentType = "application/xml";
    QnRestRequestHandlerPtr handler = QnRestProcessorPool::instance()->findHandler(url.path());
    if (handler)
    {
        if (!m_noAuth)
        {
            QnUserResourcePtr user = qnResPool->getResourceById<QnUserResource>(d->authUserId);
            if (!user)
            {
                sendUnauthorizedResponse(nx_http::StatusCode::forbidden, NOT_AUTHORIZED_HTML);
                return;
            }
            if (!qnResourceAccessManager->hasGlobalPermission(user, handler->permissions()))
            {
                sendUnauthorizedResponse(nx_http::StatusCode::forbidden, NOT_AUTHORIZED_HTML);
                return;
            }
        }

        if (d->request.requestLine.method.toUpper() == "GET") {
            rez = handler->executeGet(url.path(), params, d->response.messageBody, contentType, this);
        }
        else if (d->request.requestLine.method.toUpper() == "POST" ||
                 d->request.requestLine.method.toUpper() == "PUT") {
            rez = handler->executePost(url.path(), params, d->requestBody, nx_http::getHeaderValue(d->request.headers, "Content-Type"), d->response.messageBody, contentType, this);
        }
        else {
            qWarning() << "Unknown REST method " << d->request.requestLine.method;
            contentType = "text/plain";
            d->response.messageBody = "Invalid HTTP method";
            rez = CODE_NOT_FOUND;
        }
    }
    else {
        rez = redirectTo(QnTcpListener::defaultPage(), contentType);
    }
    QByteArray contentEncoding;
    QByteArray uncompressedResponse = d->response.messageBody;
    if ( nx_http::getHeaderValue(d->request.headers, "Accept-Encoding").toLower().contains("gzip") && !d->response.messageBody.isEmpty() && rez == CODE_OK)
    {
        if (!contentType.contains("image")) {
            d->response.messageBody = GZipCompressor::compressData(d->response.messageBody);
            contentEncoding = "gzip";
        }
    }
    nx_http::insertHeader(&d->response.headers, nx_http::HttpHeader("Cache-Control", "no-store, no-cache, must-revalidate, max-age=0"));
    nx_http::insertHeader(&d->response.headers, nx_http::HttpHeader("Cache-Control", "post-check=0, pre-check=0"));
    nx_http::insertHeader(&d->response.headers, nx_http::HttpHeader("Pragma", "no-cache"));
    sendResponse(rez, contentType, contentEncoding);
    if (handler)
        handler->afterExecute(url.path(), params, uncompressedResponse, this);
}

QnUuid QnRestConnectionProcessor::authUserId() const
{
    Q_D(const QnRestConnectionProcessor);
    return d->authUserId;
}

void QnRestConnectionProcessor::setAuthNotRequired(bool noAuth)
{
    m_noAuth = noAuth;
}

void QnRestConnectionProcessor::setAuthUserId(const QnUuid& authUserId)
{
    Q_D(QnRestConnectionProcessor);
    d->authUserId = authUserId;
}

const nx_http::Request& QnRestConnectionProcessor::request() const
{
    Q_D(const QnRestConnectionProcessor);
    return d->request;
}

nx_http::Response* QnRestConnectionProcessor::response() const
{
    Q_D(const QnRestConnectionProcessor);
    //TODO #ak remove following const_cast in 2.3.1 (requires change in QnRestRequestHandler API)
    return const_cast<nx_http::Response*>(&d->response);
}
