#include "universal_request_processor.h"

#include <QtCore/QElapsedTimer>
#include <QList>
#include <QByteArray>
#include "utils/network/tcp_connection_priv.h"
#include "universal_tcp_listener.h"
#include "universal_request_processor_p.h"
#include "authenticate_helper.h"
#include "utils/common/synctime.h"

static const int AUTH_TIMEOUT = 60 * 1000;
static const int KEEP_ALIVE_TIMEOUT = 60  * 1000;
static const int AUTHORIZED_TIMEOUT = 60 * 1000;
static const int MAX_AUTH_RETRY_COUNT = 3;


QnUniversalRequestProcessor::~QnUniversalRequestProcessor()
{
    stop();
}

QnUniversalRequestProcessor::QnUniversalRequestProcessor(QSharedPointer<AbstractStreamSocket> socket, QnTcpListener* owner, bool needAuth):
QnTCPConnectionProcessor(new QnUniversalRequestProcessorPrivate, socket)
{
    Q_D(QnUniversalRequestProcessor);
    d->processor = 0;
    d->owner = owner;
    d->needAuth = needAuth;

    setObjectName( QLatin1String("QnUniversalRequestProcessor") );
}

QnUniversalRequestProcessor::QnUniversalRequestProcessor(QnUniversalRequestProcessorPrivate* priv, QSharedPointer<AbstractStreamSocket> socket, QnTcpListener* owner, bool needAuth):
QnTCPConnectionProcessor(priv, socket)
{
    Q_D(QnUniversalRequestProcessor);
    d->processor = 0;
    d->owner = owner;
    d->needAuth = needAuth;
}

bool QnUniversalRequestProcessor::authenticate()
{
    Q_D(QnUniversalRequestProcessor);

    int retryCount = 0;
    if (d->needAuth)
    {
        QUrl url = getDecodedUrl();
        QString path = url.path().trimmed();
        if (path.endsWith(L'/'))
            path = path.left(path.size()-1);
        if (path.startsWith(L'/'))
            path = path.mid(1);
        const bool isProxy = static_cast<QnUniversalTcpListener*>(d->owner)->isProxy(d->request);
        QElapsedTimer t;
        t.restart();
        while (!qnAuthHelper->authenticate(d->request, d->response, isProxy) && d->socket->isConnected())
        {
            d->responseBody = isProxy ? STATIC_PROXY_UNAUTHORIZED_HTML: STATIC_UNAUTHORIZED_HTML;
            sendResponse(isProxy ? CODE_PROXY_AUTH_REQUIRED : CODE_AUTH_REQUIRED, "text/html");

            if (++retryCount > MAX_AUTH_RETRY_COUNT)
                return false;
            while (t.elapsed() < AUTH_TIMEOUT && d->socket->isConnected()) 
            {
                if (readRequest()) {
                    parseRequest();
                    break;
                }
            }
            if (t.elapsed() >= AUTH_TIMEOUT)
                return false; // close connection
        }
    }

    return true;
}

void QnUniversalRequestProcessor::run()
{
    Q_D(QnUniversalRequestProcessor);

    initSystemThreadId();

    if (!readRequest()) 
        return;

    QElapsedTimer t;
    t.restart();
    bool ready = true;
    bool isKeepAlive = false;

    while (1)
    {
        if (ready) 
        {
            parseRequest();

            if(!authenticate())
                return;

            d->response.headers.clear();
            isKeepAlive = nx_http::getHeaderValue( d->request.headers, "Connection" ).toLower() == "keep-alive" && d->protocol.toLower() == "http";
            if (isKeepAlive) {
                d->response.headers.insert(nx_http::HttpHeader("Connection", "Keep-Alive"));
                d->response.headers.insert(nx_http::HttpHeader("Keep-Alive", lit("timeout=%1").arg(KEEP_ALIVE_TIMEOUT/1000).toLatin1()) );
            }
            if( !processRequest() )
            {
                d->response.statusLine.version = d->request.requestLine.version;
                d->response.statusLine.statusCode = nx_http::StatusCode::notFound;
                d->response.statusLine.reasonPhrase = nx_http::StatusCode::toString( d->response.statusLine.statusCode );
                d->response.headers.insert( nx_http::HttpHeader( "Content-Type", "text/plain" ) );
                d->response.messageBody = "NOT FOUND";
                d->response.headers.insert( nx_http::HttpHeader( "Content-Length", nx_http::StringType::number(d->response.messageBody.size()) ) );
                sendBuffer( d->response.toString() );
            }
        }

        if (!d->socket)
            break; // processor has token socket ownership

        if (!isKeepAlive || t.elapsed() >= KEEP_ALIVE_TIMEOUT || !d->socket->isConnected())
            break;

        ready = readRequest();
    }
    if (d->socket)
        d->socket->close();
}

bool QnUniversalRequestProcessor::processRequest()
{
    Q_D(QnUniversalRequestProcessor);

    QMutexLocker lock(&d->mutex);
    d->processor = dynamic_cast<QnUniversalTcpListener*>(d->owner)->createNativeProcessor(d->socket, d->request.requestLine.version.protocol, d->request);
    if( !d->processor )
        return false;

    if (d->processor && !needToStop()) 
    {
        copyClientRequestTo(*d->processor);
        d->processor->execute(d->mutex);
        if (d->processor->isTakeSockOwnership())
            d->socket.clear();
        else 
            d->processor->releaseSocket();
    }
    delete d->processor;
    d->processor = 0;
    return true;
}

void QnUniversalRequestProcessor::pleaseStop()
{
    Q_D(QnUniversalRequestProcessor);
    QMutexLocker lock(&d->mutex);
    QnTCPConnectionProcessor::pleaseStop();
    if (d->processor)
        d->processor->pleaseStop();
}
