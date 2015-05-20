
#include <QtCore/QUrl>
#include <QtCore/QUrlQuery>
#include <QtCore/QRegExp>

#include "rest_connection_processor.h"
#include "utils/network/tcp_connection_priv.h"
#include "utils/network/tcp_listener.h"
#include "request_handler.h"
#include "network/authenticate_helper.h"
#include "utils/gzip/gzip_compressor.h"

void QnRestProcessorPool::registerHandler( const QString& path, QnRestRequestHandler* handler )
{
    m_handlers.insert(path, QnRestRequestHandlerPtr(handler));
    handler->setPath(path);

}

QnRestRequestHandlerPtr QnRestProcessorPool::findHandler( QString path ) const
{
    while (path.startsWith(L'/'))
        path = path.mid(1);
    if (path.endsWith(L'/'))
        path = path.left(path.length()-1);

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
    QnTCPConnectionProcessor(new QnRestConnectionProcessorPrivate, socket)
{
    Q_D(QnRestConnectionProcessor);
    d->owner = _owner;
}

QnRestConnectionProcessor::~QnRestConnectionProcessor()
{
    stop();
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


    d->responseBody.clear();

    QUrl url = getDecodedUrl();
    QString path = url.path();
    QList<QPair<QString, QString> > params = QUrlQuery(url.query()).queryItems(QUrl::FullyDecoded);
    int rez = CODE_OK;
    QByteArray contentType = "application/xml";
    QnRestRequestHandlerPtr handler = QnRestProcessorPool::instance()->findHandler(url.path());
    if (handler) 
    {
        if (d->request.requestLine.method.toUpper() == "GET") {
            rez = handler->executeGet(url.path(), params, d->responseBody, contentType, this);
        }
        else if (d->request.requestLine.method.toUpper() == "POST") {
            rez = handler->executePost(url.path(), params, d->requestBody, nx_http::getHeaderValue(d->request.headers, "Content-Type"), d->responseBody, contentType, this);
        }
        else {
            qWarning() << "Unknown REST method " << d->request.requestLine.method;
            contentType = "text/plain";
            d->responseBody = "Invalid HTTP method";
            rez = CODE_NOT_FOUND;
        }
    }
    else {
        rez = redirectTo(QnTcpListener::defaultPage(), contentType);
    }
    QByteArray contentEncoding;
    if ( nx_http::getHeaderValue(d->request.headers, "Accept-Encoding").toLower().contains("gzip") && !d->responseBody.isEmpty() && rez == CODE_OK) 
    {
        if (!contentType.contains("image")) {
            d->responseBody = GZipCompressor::compressData(d->responseBody);
            contentEncoding = "gzip";
        }
    }
    nx_http::insertHeader(&d->response.headers, nx_http::HttpHeader("Cache-Control", "no-store, no-cache, must-revalidate, max-age=0"));
    nx_http::insertHeader(&d->response.headers, nx_http::HttpHeader("Cache-Control", "post-check=0, pre-check=0"));
    nx_http::insertHeader(&d->response.headers, nx_http::HttpHeader("Pragma", "no-cache"));
    sendResponse(rez, contentType, contentEncoding, false);
}

QnUuid QnRestConnectionProcessor::authUserId() const
{
    Q_D(const QnRestConnectionProcessor);
    return d->authUserId;
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
