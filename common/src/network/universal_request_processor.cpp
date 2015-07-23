#include "universal_request_processor.h"

#include <QtCore/QElapsedTimer>
#include <QList>
#include <QByteArray>
#include "utils/gzip/gzip_compressor.h"
#include "utils/network/tcp_connection_priv.h"
#include "universal_request_processor_p.h"
#include "authenticate_helper.h"
#include "utils/common/synctime.h"
#include "common/common_module.h"
#include "http/custom_headers.h"


static const int AUTH_TIMEOUT = 60 * 1000;
//static const int AUTHORIZED_TIMEOUT = 60 * 1000;
static const int MAX_AUTH_RETRY_COUNT = 3;


QnUniversalRequestProcessor::~QnUniversalRequestProcessor()
{
    stop();
}

QnUniversalRequestProcessor::QnUniversalRequestProcessor(
        QSharedPointer<AbstractStreamSocket> socket,
        QnUniversalTcpListener* owner, bool needAuth)
    : QnTCPConnectionProcessor(new QnUniversalRequestProcessorPrivate, socket)
{
    Q_D(QnUniversalRequestProcessor);
    d->processor = 0;
    d->owner = owner;
    d->needAuth = needAuth;

    setObjectName( QLatin1String("QnUniversalRequestProcessor") );
}

QnUniversalRequestProcessor::QnUniversalRequestProcessor(
        QnUniversalRequestProcessorPrivate* priv, QSharedPointer<AbstractStreamSocket> socket,
        QnUniversalTcpListener* owner, bool needAuth)
    : QnTCPConnectionProcessor(priv, socket)
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
        const bool isProxy = d->owner->isProxy(d->request);
        QElapsedTimer t;
        t.restart();
        while (!qnAuthHelper->authenticate(d->request, d->response, isProxy, userId))
        {
            if( !d->socket->isConnected() )
                return false;   //connection has been closed

            if( d->request.requestLine.method == nx_http::Method::GET ||
                d->request.requestLine.method == nx_http::Method::HEAD )
            {
                d->response.messageBody = isProxy ? STATIC_PROXY_UNAUTHORIZED_HTML: unauthorizedPageBody();
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
                    if( !d->response.messageBody.isEmpty() )
                        d->response.messageBody = GZipCompressor::compressData(d->response.messageBody);
                }
                else
                {
                    //TODO #ak not supported encoding requested
                }
            }
            sendResponse(
                isProxy ? CODE_PROXY_AUTH_REQUIRED : CODE_AUTH_REQUIRED,
                d->response.messageBody.isEmpty() ? QByteArray() : "text/html; charset=utf-8",
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

            auto handler = d->owner->findHandler(d->protocol, d->request);
            if (handler && !authenticate(&d->authUserId))
                return;

            d->response.headers.clear();

            isKeepAlive = isConnectionCanBePersistent();


            // getting a new handler inside is necessary due to possibility of
            // changing request during authentication
            if (!processRequest())
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

    QnMutexLocker lock( &d->mutex ); 
    if (auto handler = d->owner->findHandler(d->protocol, d->request))
        d->processor = handler(d->socket, d->owner);
    else
        return false;

    if ( !needToStop() )
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
    QnMutexLocker lock( &d->mutex );
    QnTCPConnectionProcessor::pleaseStop();
    if (d->processor)
        d->processor->pleaseStop();
}
