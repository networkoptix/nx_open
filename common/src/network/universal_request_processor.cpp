#include "universal_request_processor.h"

#include <QtCore/QElapsedTimer>
#include <QList>
#include <QByteArray>
#include "utils/network/tcp_connection_priv.h"
#include "universal_tcp_listener.h"
#include "universal_request_processor_p.h"
#include "authenticate_helper.h"

static const int AUTH_TIMEOUT = 60 * 1000;
static const int KEEP_ALIVE_TIMEOUT = 60  * 1000;

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

    if (d->needAuth &&  d->protocol.toLower() == "http")
    {
        QUrl url = getDecodedUrl();
        QString path = url.path().trimmed();
        if (path.endsWith(L'/'))
            path = path.left(path.size()-1);
        if (path.startsWith(L'/'))
            path = path.mid(1);
        bool needAuth = path != lit("api/ping");

        QTime t;
        t.restart();
        while (needAuth && !qnAuthHelper->authenticate(d->request, d->response))
        {
            d->responseBody = STATIC_UNAUTHORIZED_HTML;
            sendResponse("HTTP", CODE_AUTH_REQUIRED, "text/html");
            while (t.elapsed() < AUTH_TIMEOUT) 
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

    saveSysThreadID();

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
                d->response.headers.insert(nx_http::HttpHeader("Keep-Alive", QString::fromLatin1("timeout=%1").arg(KEEP_ALIVE_TIMEOUT/1000).toLatin1()) );
            }
            processRequest();
        }

        if (!d->socket)
            break; // processor has token socket ownership

        bool isConnected = d->socket->isConnected();
        if (!isKeepAlive || t.elapsed() >= KEEP_ALIVE_TIMEOUT || !d->socket->isConnected())
            break;

        ready = readRequest();
    }
    if (d->socket)
        d->socket->close();
}

void QnUniversalRequestProcessor::processRequest()
{
    Q_D(QnUniversalRequestProcessor);
    QList<QByteArray> header = d->clientRequest.left(d->clientRequest.indexOf('\n')).split(' ');
    if (header.size() > 2) 
    {
        QByteArray protocol = header[2].split('/')[0].toUpper();
        QMutexLocker lock(&d->mutex);
        d->processor = dynamic_cast<QnUniversalTcpListener*>(d->owner)->createNativeProcessor(d->socket, protocol, QUrl(QString::fromUtf8(header[1])).path());
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
    }
}

void QnUniversalRequestProcessor::pleaseStop()
{
    Q_D(QnUniversalRequestProcessor);
    QMutexLocker lock(&d->mutex);
    QnTCPConnectionProcessor::pleaseStop();
    if (d->processor)
        d->processor->pleaseStop();
}
