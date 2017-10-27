#include "universal_request_processor.h"

#include <QtCore/QElapsedTimer>
#include <QList>
#include <QByteArray>

#include "audit/audit_manager.h"
#include "common/common_module.h"
#include "core/resource_management/resource_pool.h"
#include <nx/network/http/custom_headers.h>
#include "network/tcp_connection_priv.h"
#include "universal_request_processor_p.h"
#include <nx/fusion/model_functions.h>
#include "utils/common/synctime.h"
#include <nx/utils/gzip/gzip_compressor.h>
#include <nx/network/flash_socket/types.h>
#include <rest/server/rest_connection_processor.h>
#include <utils/common/app_info.h>

namespace {

static const int AUTH_TIMEOUT = 60 * 1000;
static const int MAX_AUTH_RETRY_COUNT = 3;

bool isDigestAuth(const nx_http::Response& response)
{
    const auto header = response.headers.find(nx_http::header::WWWAuthenticate::NAME);
    if (header == response.headers.end())
        return false;
    nx_http::header::WWWAuthenticate wwwAuthenticate;
    wwwAuthenticate.parse(header->second);
    return wwwAuthenticate.authScheme == nx_http::header::AuthScheme::digest;
}

} // namespace

QnUniversalRequestProcessor::~QnUniversalRequestProcessor()
{
    stop();
}

QnUniversalRequestProcessor::QnUniversalRequestProcessor(
    QSharedPointer<AbstractStreamSocket> socket,
    QnUniversalTcpListener* owner, bool needAuth)
: 
    QnTCPConnectionProcessor(new QnUniversalRequestProcessorPrivate, socket, owner)
{
    Q_D(QnUniversalRequestProcessor);
    d->processor = 0;
    d->needAuth = needAuth;

    setObjectName( QLatin1String("QnUniversalRequestProcessor") );
}

QnUniversalRequestProcessor::QnUniversalRequestProcessor(
    QnUniversalRequestProcessorPrivate* priv, 
    QSharedPointer<AbstractStreamSocket> socket,
    QnUniversalTcpListener* owner, 
    bool needAuth)
: 
    QnTCPConnectionProcessor(priv, socket, owner)
{
    Q_D(QnUniversalRequestProcessor);
    d->processor = 0;
    d->needAuth = needAuth;
}

static QByteArray m_unauthorizedPageBody = STATIC_UNAUTHORIZED_HTML;
static nx_http::AuthMethod::Values m_unauthorizedPageForMethods = nx_http::AuthMethod::NotDefined;

void QnUniversalRequestProcessor::setUnauthorizedPageBody(const QByteArray& value, nx_http::AuthMethod::Values methods)
{
    m_unauthorizedPageBody = value;
    m_unauthorizedPageForMethods = methods;
}

QByteArray QnUniversalRequestProcessor::unauthorizedPageBody()
{
    return m_unauthorizedPageBody;
}

bool QnUniversalRequestProcessor::authenticate(Qn::UserAccessData* accessRights, bool *noAuth)
{
    Q_D(QnUniversalRequestProcessor);

    int retryCount = 0;
    bool logReported = false;
    if (d->needAuth)
    {
        QUrl url = getDecodedUrl();
        // set variable to true if standard proxy_unauthorized should be used
        const bool isProxy = needStandardProxy(d->owner->commonModule(), d->request);
        QElapsedTimer t;
        t.restart();
        nx_http::AuthMethod::Value usedMethod = nx_http::AuthMethod::noAuth;
        Qn::AuthResult authResult;
        while ((authResult = qnAuthHelper->authenticate(d->request, d->response, isProxy, accessRights, &usedMethod)) != Qn::Auth_OK)
        {
            nx_http::insertOrReplaceHeader(
                &d->response.headers,
                nx_http::HttpHeader( Qn::AUTH_RESULT_HEADER_NAME, QnLexical::serialized(authResult).toUtf8() ) );

            int retryThreshold = 0;
            if (isDigestAuth(d->response))
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

            nx_http::StatusCode::Value httpResult;
            QByteArray msgBody;
            if (isProxy)
            {
                msgBody = STATIC_PROXY_UNAUTHORIZED_HTML;
                httpResult = nx_http::StatusCode::proxyAuthenticationRequired;
            }
            else if (authResult ==  Qn::Auth_Forbidden)
            {
                msgBody = STATIC_FORBIDDEN_HTML;
                httpResult = nx_http::StatusCode::forbidden;
            }
            else
            {
                if (usedMethod & m_unauthorizedPageForMethods)
                    msgBody = unauthorizedPageBody();
                else
                    msgBody = STATIC_UNAUTHORIZED_HTML;
                httpResult = nx_http::StatusCode::unauthorized;
            }
            sendUnauthorizedResponse(httpResult, msgBody);


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
        if (qnAuthHelper->restrictionList()->getAllowedAuthMethods(d->request) & nx_http::AuthMethod::noAuth)
            *noAuth = true;
        if (usedMethod != nx_http::AuthMethod::noAuth)
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
        if (ready)
        {
            t.restart();
            parseRequest();
            auto owner = static_cast<QnUniversalTcpListener*> (d->owner);
            const auto redirect = owner->processorPool()->getRedirectRule(
                d->request.requestLine.url.path());
            if (redirect)
            {
                QByteArray contentType;
                int rez = redirectTo(redirect->toUtf8(), contentType);
                sendResponse(rez, contentType);
            }
            else
            {
                auto owner = static_cast<QnUniversalTcpListener*> (d->owner);
                auto handler = owner->findHandler(d->protocol, d->request);
                bool noAuth = false;
                if (handler)
                {
                    if (owner->isAuthentificationRequired(d->request)
                        && !authenticate(&d->accessRights, &noAuth))
                    {
                        return;
                    }
                }

                isKeepAlive = isConnectionCanBePersistent();

                // getting a new handler inside is necessary due to possibility of
                // changing request during authentication
                if (!processRequest(noAuth))
                {
                    QByteArray contentType;
                    int rez = notFound(contentType);
                    sendResponse(rez, contentType);
                }
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

bool QnUniversalRequestProcessor::processRequest(bool noAuth)
{
    Q_D(QnUniversalRequestProcessor);

    QnMutexLocker lock( &d->mutex );
    auto owner = static_cast<QnUniversalTcpListener*> (d->owner);
    if (auto handler = owner->findHandler(d->protocol, d->request))
        d->processor = handler(d->socket, owner);
    else
        return false;

    auto restProcessor = dynamic_cast<QnRestConnectionProcessor*>(d->processor);
    if (restProcessor)
        restProcessor->setAuthNotRequired(noAuth);
    if ( !needToStop() )
    {
        copyClientRequestTo(*d->processor);
        if (d->processor->isSocketTaken())
            d->socket.clear(); // some of handlers have addition thread and depend of socket destructor. We should clear socket immediately to prevent race condition
        d->processor->execute(d->mutex);
        if (!d->processor->isSocketTaken())
            d->processor->releaseSocket();
        else
            d->socket.clear(); // some of handlers set ownership dynamically during a execute call. So, check it again.
    }

    delete d->processor;
    d->processor = 0;
    return true;
}

void QnUniversalRequestProcessor::pleaseStop()
{
    Q_D(QnUniversalRequestProcessor);
    QnMutexLocker lock( &d->mutex );
    if (d->processor)
        d->processor->pleaseStop();
    QnTCPConnectionProcessor::pleaseStop();
}

bool QnUniversalRequestProcessor::isProxy(QnCommonModule* commonModule, const nx_http::Request& request)
{
    nx_http::HttpHeaders::const_iterator xServerGuidIter = request.headers.find( Qn::SERVER_GUID_HEADER_NAME );
    if( xServerGuidIter != request.headers.end() )
    {
        // is proxy to other media server
        QnUuid desiredServerGuid(xServerGuidIter->second);
        if (desiredServerGuid != commonModule->moduleGUID())
            return true;
    }

    return needStandardProxy(commonModule, request);
}

bool QnUniversalRequestProcessor::needStandardProxy(QnCommonModule* commonModule, const nx_http::Request& request)
{
    return isCloudRequest(request) || isProxyForCamera(commonModule, request);
}

bool QnUniversalRequestProcessor::isCloudRequest(const nx_http::Request& request)
{
    return request.requestLine.url.host() == nx::network::AppInfo::defaultCloudHost() ||
           request.requestLine.url.path().startsWith("/cdb") ||
           request.requestLine.url.path().startsWith("/nxcloud") ||
           request.requestLine.url.path().startsWith("/nxlicense");
}

bool QnUniversalRequestProcessor::isProxyForCamera(
    QnCommonModule* commonModule,
    const nx_http::Request& request)
{
    nx_http::BufferType desiredCameraGuid;
    nx_http::HttpHeaders::const_iterator xCameraGuidIter = request.headers.find( Qn::CAMERA_GUID_HEADER_NAME );
    if( xCameraGuidIter != request.headers.end() )
    {
        desiredCameraGuid = xCameraGuidIter->second;
    }
    else {
        desiredCameraGuid = request.getCookieValue(Qn::CAMERA_GUID_HEADER_NAME);
    }
    if (!desiredCameraGuid.isEmpty()) {
        QnResourcePtr camera = commonModule->resourcePool()->getResourceById(QnUuid::fromStringSafe(desiredCameraGuid));
        return camera != 0;
    }

    return false;
}
