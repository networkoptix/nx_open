#include "universal_request_processor.h"

#include <QtCore/QElapsedTimer>
#include <QList>
#include <QByteArray>
#include "utils/gzip/gzip_compressor.h"
#include "utils/network/tcp_connection_priv.h"
#include "universal_tcp_listener.h"
#include "universal_request_processor_p.h"
#include "authenticate_helper.h"
#include "utils/common/synctime.h"
#include "common/common_module.h"
#include "http/custom_headers.h"


static const int AUTH_TIMEOUT = 60 * 1000;
static const int KEEP_ALIVE_TIMEOUT = 60  * 1000;
//static const int AUTHORIZED_TIMEOUT = 60 * 1000;
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

static QByteArray m_unauthorizedPageBody = STATIC_UNAUTHORIZED_HTML;

void QnUniversalRequestProcessor::setUnauthorizedPageBody(const QByteArray& value)
{
    m_unauthorizedPageBody = value;
}

QByteArray QnUniversalRequestProcessor::unauthorizedPageBody()
{
    return m_unauthorizedPageBody;
}

bool QnUniversalRequestProcessor::authenticate(QnUuid* userId)
{
    Q_D(QnUniversalRequestProcessor);

    int retryCount = 0;
    if (d->needAuth)
    {
        QUrl url = getDecodedUrl();
        const bool isProxy = static_cast<QnUniversalTcpListener*>(d->owner)->isProxy(d->request);
        QElapsedTimer t;
        t.restart();
        while (!qnAuthHelper->authenticate(d->request, d->response, isProxy, userId) && d->socket->isConnected())
        {
            if( d->request.requestLine.method == nx_http::Method::GET ||
                d->request.requestLine.method == nx_http::Method::HEAD )
            {
                d->responseBody = isProxy ? STATIC_PROXY_UNAUTHORIZED_HTML: unauthorizedPageBody();
            }
            if (nx_http::getHeaderValue( d->response.headers, Qn::SERVER_GUID_HEADER_NAME ).isEmpty())
                d->response.headers.insert(nx_http::HttpHeader(Qn::SERVER_GUID_HEADER_NAME, qnCommon->moduleGUID().toByteArray()));

            auto acceptEncodingHeaderIter = d->request.headers.find( "Accept-Encoding" );
            QByteArray contentEncoding;
            if( acceptEncodingHeaderIter != d->request.headers.end() )
            {
                nx_http::header::AcceptEncodingHeader acceptEncodingHeader( acceptEncodingHeaderIter->second );
                if( acceptEncodingHeader.encodingIsAllowed( "identity" ) )
                {
                    contentEncoding = "identity";
                }
                else if( acceptEncodingHeader.encodingIsAllowed( "gzip" ) )
                {
                    contentEncoding = "gzip";
                    if( !d->responseBody.isEmpty() )
                        d->responseBody = GZipCompressor::compressData(d->responseBody);
                }
                else
                {
                    //TODO #ak not supported encoding requested
                }
            }
            sendResponse(
                isProxy ? CODE_PROXY_AUTH_REQUIRED : CODE_AUTH_REQUIRED,
                d->responseBody.isEmpty() ? QByteArray() : "text/html",
                contentEncoding );

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
            t.restart();
            parseRequest();

            bool isHandlerExist = dynamic_cast<QnUniversalTcpListener*>(d->owner)->findHandler(d->socket, d->request.requestLine.version.protocol, d->request) != 0;
            if (isHandlerExist && !authenticate(&d->authUserId))
                return;

            d->response.headers.clear();

            if( d->request.requestLine.version == nx_http::http_1_1 )
                isKeepAlive = nx_http::getHeaderValue( d->request.headers, "Connection" ).toLower() != "close";
            else if( d->request.requestLine.version == nx_http::http_1_0 )
                isKeepAlive = nx_http::getHeaderValue( d->request.headers, "Connection" ).toLower() == "keep-alive";

            if (isKeepAlive) {
                d->response.headers.insert(nx_http::HttpHeader("Connection", "Keep-Alive"));
                d->response.headers.insert(nx_http::HttpHeader("Keep-Alive", lit("timeout=%1").arg(KEEP_ALIVE_TIMEOUT/1000).toLatin1()) );
            }
            if( !processRequest() )
            {
                QByteArray contentType;
                int rez = redirectTo(QnTcpListener::defaultPage(), contentType);
                sendResponse(rez, contentType);
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
