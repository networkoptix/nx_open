#include "universal_request_processor.h"

#include <QtCore/QElapsedTimer>
#include <QList>
#include <QByteArray>
#include "utils/gzip/gzip_compressor.h"
#include "utils/network/tcp_connection_priv.h"
#include "universal_request_processor_p.h"
#include "utils/common/synctime.h"
#include "common/common_module.h"
#include "http/custom_headers.h"
#include "utils/network/flash_socket/types.h"
#include "audit/audit_manager.h"
#include <utils/common/model_functions.h>

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
static AuthMethod::Values m_unauthorizedPageForMethods = AuthMethod::NotDefined;

void QnUniversalRequestProcessor::setUnauthorizedPageBody(const QByteArray& value, AuthMethod::Values methods)
{
    m_unauthorizedPageBody = value;
    m_unauthorizedPageForMethods = methods;
}

QByteArray QnUniversalRequestProcessor::unauthorizedPageBody()
{
    return m_unauthorizedPageBody;
}

bool QnUniversalRequestProcessor::authenticate(QnUuid* userId)
{
    Q_D(QnUniversalRequestProcessor);

    int retryCount = 0;
    bool logReported = false;
    if (d->needAuth)
    {
        QUrl url = getDecodedUrl();
        //not asking Proxy authentication because we proxy requests in a custom way
        const bool isProxy = false;
        QElapsedTimer t;
        t.restart();
        AuthMethod::Value usedMethod = AuthMethod::noAuth;
        Qn::AuthResult authResult;
        while ((authResult = qnAuthHelper->authenticate(d->request, d->response, isProxy, userId, &usedMethod)) != Qn::Auth_OK)
        {
            nx_http::insertOrReplaceHeader(
                &d->response.headers,
                nx_http::HttpHeader( Qn::AUTH_RESULT_HEADER_NAME, QnLexical::serialized(authResult).toUtf8() ) );

            int retryThreshold = 0;
            if (authResult == Qn::Auth_WrongDigest)
                retryThreshold = MAX_AUTH_RETRY_COUNT;
            else if (d->authenticatedOnce)
                retryThreshold = 2; // Allow two more try if password just changed (QT client need it because of password cache)
            if (retryCount >= retryThreshold && !logReported && authResult != Qn::Auth_WrongInternalLogin)
            {   
                logReported = true;
                auto session = authSession();
                session.id = QnUuid::createUuid();
                qnAuditManager->addAuditRecord(qnAuditManager->prepareRecord(session, Qn::AR_UnauthorizedLogin));
            }

            if( !d->socket->isConnected() )
                return false;   //connection has been closed

            if( d->request.requestLine.method == nx_http::Method::GET ||
                d->request.requestLine.method == nx_http::Method::HEAD )
            {
                if (isProxy)
                    d->response.messageBody = STATIC_PROXY_UNAUTHORIZED_HTML;
                else if (usedMethod & m_unauthorizedPageForMethods)
                    d->response.messageBody = unauthorizedPageBody();
                else
                    d->response.messageBody = STATIC_UNAUTHORIZED_HTML;
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

            if (++retryCount > MAX_AUTH_RETRY_COUNT) {
                return false;
            }
            while (t.elapsed() < AUTH_TIMEOUT && d->socket->isConnected()) 
            {
                if (readRequest()) {
                    parseRequest();
                    break;
                }
            }
            if (t.elapsed() >= AUTH_TIMEOUT || !d->socket->isConnected())
                return false; // close connection
        }
        if (usedMethod != AuthMethod::noAuth)
            d->authenticatedOnce = true;
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
        if( d->clientRequest.startsWith(nx_flash_sock::POLICY_FILE_REQUEST_NAME) )
        {
            QFile f( QString::fromLatin1(nx_flash_sock::CROSS_DOMAIN_XML_PATH) );
            if( f.open( QIODevice::ReadOnly ) )
                sendData( f.readAll() );
            break;
        }

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

    QMutexLocker lock(&d->mutex); 
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
    QMutexLocker lock(&d->mutex);
    QnTCPConnectionProcessor::pleaseStop();
    if (d->processor)
        d->processor->pleaseStop();
}
