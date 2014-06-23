
#include <QtCore/QUrl>
#include <QtCore/QUrlQuery>
#include <QtCore/QRegExp>

#include "rest_connection_processor.h"
#include "utils/network/tcp_connection_priv.h"
#include "utils/network/tcp_listener.h"
#include "rest_server.h"
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
    if (path.startsWith(L'/'))
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

static QnRestProcessorPool* QnRestProcessorPool_instance = nullptr;

void QnRestProcessorPool::initStaticInstance( QnRestProcessorPool* _instance )
{
    QnRestProcessorPool_instance = _instance;
}

QnRestProcessorPool* QnRestProcessorPool::instance()
{
    return QnRestProcessorPool_instance;
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

void QnRestConnectionProcessor::createHelpPage()
{
    Q_D(QnRestConnectionProcessor);

    d->responseBody.clear();
    d->responseBody.append("<!DOCTYPE html PUBLIC \"-//W3C//DTD XHTML 1.0 Transitional//EN\" \"http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd\">\n");
    d->responseBody.append("<html lang=\"en\" xmlns=\"http://www.w3.org/1999/xhtml\">\n");
    d->responseBody.append("<head>\n");
    d->responseBody.append("<b>Requested method is absent. Allowed methods:</b>\n");
    d->responseBody.append("</head>\n");
    d->responseBody.append("<body>\n");

    d->responseBody.append("<TABLE BORDER=\"1\" CELLSPACING=\"0\">\n");
    for( QnRestProcessorPool::Handlers::const_iterator
        itr = QnRestProcessorPool::instance()->handlers().begin();
        itr != QnRestProcessorPool::instance()->handlers().end();
    ++itr)
    {
        QString str = itr.key();
        //if (str.startsWith(QLatin1String("api/")))
        {
            d->responseBody.append("<TR><TD>");
            d->responseBody.append(str.toLatin1());
            d->responseBody.append("<TD>");
            //d->responseBody.append(itr.value()->description());
            d->responseBody.append("</TD>");
            d->responseBody.append("</TD></TR>\n");
        }
    }
    d->responseBody.append("</TABLE>\n");

    d->responseBody.append("</body>\n");
    d->responseBody.append("</html>\n");
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
            rez = handler->executeGet(url.path(), params, d->responseBody, contentType);
        }
        else if (d->request.requestLine.method.toUpper() == "POST") {
            rez = handler->executePost(url.path(), params, d->requestBody, d->responseBody, contentType);
        }
        else {
            qWarning() << "Unknown REST method " << d->request.requestLine.method;
            contentType = "text/plain";
            d->responseBody = "Invalid HTTP method";
            rez = CODE_NOT_FOUND;
        }
    }
    else {
        qWarning() << "Unknown REST path " << url.path();
        contentType = "text/html";
        rez = CODE_NOT_FOUND;
        createHelpPage();
    }
    QByteArray contentEncoding;
    if ( nx_http::getHeaderValue(d->request.headers, "Accept-Encoding").toLower().contains("gzip") && !d->responseBody.isEmpty()) 
    {
        if (!contentType.contains("image")) {
            d->responseBody = GZipCompressor::compressData(d->responseBody);
            contentEncoding = "gzip";
        }
    }
    sendResponse(rez, contentType, contentEncoding, false);
}
