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
#include <nx/utils/log/log.h>
#include <api/global_settings.h>
#include <nx/network/rtsp/rtsp_types.h>

namespace {

static const int AUTH_TIMEOUT = 60 * 1000;
static const int MAX_AUTH_RETRY_COUNT = 3;


} // namespace

QnUniversalRequestProcessor::~QnUniversalRequestProcessor()
{
    stop();
}

QnUniversalRequestProcessor::QnUniversalRequestProcessor(
    QSharedPointer<nx::network::AbstractStreamSocket> socket,
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
    QSharedPointer<nx::network::AbstractStreamSocket> socket,
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
static nx::network::http::AuthMethod::Values m_unauthorizedPageForMethods = nx::network::http::AuthMethod::NotDefined;

void QnUniversalRequestProcessor::setUnauthorizedPageBody(const QByteArray& value, nx::network::http::AuthMethod::Values methods)
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
        nx::utils::Url url = getDecodedUrl();
        // set variable to true if standard proxy_unauthorized should be used
        const bool isProxy = needStandardProxy(d->owner->commonModule(), d->request);
        QElapsedTimer t;
        t.restart();
        nx::network::http::AuthMethod::Value usedMethod = nx::network::http::AuthMethod::noAuth;
        Qn::AuthResult authResult;
        QnAuthSession lastUnauthorizedData;
        const auto clientIp = d->socket->getForeignAddress().address;
        while ((authResult = qnAuthHelper->authenticate(
            clientIp, d->request, d->response, isProxy, accessRights, &usedMethod)) != Qn::Auth_OK)
        {
            lastUnauthorizedData = authSession();

            nx::network::http::insertOrReplaceHeader(
                &d->response.headers,
                nx::network::http::HttpHeader( Qn::AUTH_RESULT_HEADER_NAME, QnLexical::serialized(authResult).toUtf8() ) );


            if( !d->socket->isConnected() )
                break;   //connection has been closed

            nx::network::http::StatusCode::Value httpResult;
            QByteArray msgBody;
            if (isProxy)
            {
                msgBody = STATIC_PROXY_UNAUTHORIZED_HTML;
                httpResult = nx::network::http::StatusCode::proxyAuthenticationRequired;
            }
            else if (authResult ==  Qn::Auth_Forbidden)
            {
                msgBody = STATIC_FORBIDDEN_HTML;
                httpResult = nx::network::http::StatusCode::forbidden;
            }
            else
            {
                if (usedMethod & m_unauthorizedPageForMethods)
                    msgBody = unauthorizedPageBody();
                else
                    msgBody = STATIC_UNAUTHORIZED_HTML;
                httpResult = nx::network::http::StatusCode::unauthorized;
            }
            sendUnauthorizedResponse(httpResult, msgBody);


            if (++retryCount > MAX_AUTH_RETRY_COUNT) {
                break;
            }
            while (t.elapsed() < AUTH_TIMEOUT && d->socket->isConnected())
            {
                if (readRequest()) {
                    parseRequest();
                    break;
                }
            }
            if (t.elapsed() >= AUTH_TIMEOUT || !d->socket->isConnected())
                break; // close connection
        }

        if (usedMethod == nx::network::http::AuthMethod::noAuth)
        {
            *noAuth = true;
        }
        else if (authResult != Qn::Auth_OK)
        {
            if (authResult != Qn::Auth_WrongInternalLogin)
            {
                lastUnauthorizedData.id = QnUuid::createUuid();
                qnAuditManager->addAuditRecord(qnAuditManager->prepareRecord(
                    lastUnauthorizedData, Qn::AR_UnauthorizedLogin));
            }
            return false;
        }
        else
        {
            d->authenticatedOnce = true;
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
            auto owner = static_cast<QnUniversalTcpListener*> (d->owner);

            // TODO: Consider to move this interface into base stream socket class.
            const auto secureSocket = dynamic_cast<nx::network::AbstractEncryptedStreamSocket*>(
                d->socket.data());
            if (!secureSocket || !secureSocket->isEncryptionEnabled())
            {
                const auto settings = commonModule()->globalSettings();
                const auto protocol = d->request.requestLine.version.protocol.toUpper();
                const auto redurectToScheme =
                    [&](auto scheme)
                    {
                        const auto schemeString = QString::fromUtf8(scheme);
                        nx::utils::Url url(d->request.requestLine.url);
                        if (url.scheme() == schemeString)
                        {
                            QByteArray contentType;
                            int rez = notFound(contentType);
                            return sendResponse(rez, contentType);
                        }

                        url.setScheme(schemeString);
                        if (url.host().isEmpty())
                        {
                            const auto endpoint = d->socket->getLocalAddress();
                            url.setHost(endpoint.address.toString());
                            url.setPort(endpoint.port);
                        }

                        QByteArray contentType;
                        int rez = redirectTo(url.toString().toUtf8(), contentType);
                        sendResponse(rez, contentType);
                    };

                if (protocol == "HTTP" && settings->isTrafficEncriptionForced())
                    return redurectToScheme(nx::network::http::kSecureUrlSchemeName);

                if (protocol == "RTSP" && settings->isVideoTrafficEncriptionForced())
                    return redurectToScheme(nx_rtsp::kSecureUrlSchemeName);
            }


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

bool QnUniversalRequestProcessor::isProxy(QnCommonModule* commonModule, const nx::network::http::Request& request)
{
    nx::network::http::HttpHeaders::const_iterator xServerGuidIter = request.headers.find( Qn::SERVER_GUID_HEADER_NAME );
    if( xServerGuidIter != request.headers.end() )
    {
        // is proxy to other media server
        QnUuid desiredServerGuid(xServerGuidIter->second);
        if (desiredServerGuid != commonModule->moduleGUID())
        {
            NX_VERBOSE(typeid(QnUniversalRequestProcessor),
                lm("Need proxy to another server for request [%1]").arg(request.requestLine));

            return true;
        }
    }

    return needStandardProxy(commonModule, request);
}

bool QnUniversalRequestProcessor::needStandardProxy(QnCommonModule* commonModule, const nx::network::http::Request& request)
{
    return isCloudRequest(request) || isProxyForCamera(commonModule, request);
}

bool QnUniversalRequestProcessor::isCloudRequest(const nx::network::http::Request& request)
{
    return request.requestLine.url.host() == nx::network::SocketGlobals::cloud().cloudHost() ||
           request.requestLine.url.path().startsWith("/cdb") ||
           request.requestLine.url.path().startsWith("/nxcloud") ||
           request.requestLine.url.path().startsWith("/nxlicense");
}

bool QnUniversalRequestProcessor::isProxyForCamera(
    QnCommonModule* commonModule,
    const nx::network::http::Request& request)
{
    nx::network::http::BufferType desiredCameraGuid;
    nx::network::http::HttpHeaders::const_iterator xCameraGuidIter = request.headers.find( Qn::CAMERA_GUID_HEADER_NAME );
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
