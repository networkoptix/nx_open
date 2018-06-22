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
#include <nx/network/cloud/cloud_connect_controller.h>
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
    d->listener = owner;
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
        QnAuthSession lastUnauthorizedData;
        const auto clientIp = d->socket->getForeignAddress().address;
        nx::mediaserver::Authenticator::Result result;
        while ((result = d->listener->authenticator()->tryAllMethods(
            clientIp, d->request, &d->response, isProxy)).code != Qn::Auth_OK)
        {
            lastUnauthorizedData = authSession();
            nx::network::http::insertOrReplaceHeader(
                &d->response.headers,
                {Qn::AUTH_RESULT_HEADER_NAME, QnLexical::serialized(result.code).toUtf8()});

            if( !d->socket->isConnected() )
                break;   //connection has been closed

            nx::network::http::StatusCode::Value httpResult;
            QByteArray msgBody;
            if (isProxy)
            {
                msgBody = STATIC_PROXY_UNAUTHORIZED_HTML;
                httpResult = nx::network::http::StatusCode::proxyAuthenticationRequired;
            }
            else if (result.code ==  Qn::Auth_Forbidden)
            {
                msgBody = STATIC_FORBIDDEN_HTML;
                httpResult = nx::network::http::StatusCode::forbidden;
            }
            else
            {
                if (result.usedMethods & m_unauthorizedPageForMethods)
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

        *accessRights = result.access;
        if (result.usedMethods == nx::network::http::AuthMethod::noAuth)
        {
            *noAuth = true;
        }
        else if (result.code != Qn::Auth_OK)
        {
            if (result.code != Qn::Auth_WrongInternalLogin)
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
            if (hasSecurityIssue())
                return;

            auto owner = static_cast<QnUniversalTcpListener*> (d->owner);
            if (const auto redirect = owner->processorPool()->getRedirectRule(
                    d->request.requestLine.url.path()))
            {
                QByteArray contentType;
                int code = redirectTo(redirect->toUtf8(), contentType);
                sendResponse(code, contentType);
            }
            else
            {
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
                    int code = notFound(contentType);
                    sendResponse(code, contentType);
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

bool QnUniversalRequestProcessor::hasSecurityIssue()
{
    Q_D(QnUniversalRequestProcessor);
    const auto secureSocket = dynamic_cast<nx::network::AbstractEncryptedStreamSocket*>(
        d->socket.data());
    if (!secureSocket || !secureSocket->isEncryptionEnabled())
    {
        const auto settings = commonModule()->globalSettings();
        const auto protocol = d->request.requestLine.version.protocol.toUpper();

        if (protocol == "HTTP")
        {
            if (settings->isTrafficEncriptionForced())
                return redicrectToScheme(nx::network::http::kSecureUrlSchemeName);
        }
        else if (protocol == "RTSP")
        {
            if (settings->isVideoTrafficEncriptionForced())
                return redicrectToScheme(nx_rtsp::kSecureUrlSchemeName);
        }
        else if (settings->isTrafficEncriptionForced())
        {
            NX_ASSERT(false, lm("Unable to redirect protocol to secure version: %1").arg(protocol));
            d->response.messageBody = STATIC_FORBIDDEN_HTML;
            sendResponse(CODE_FORBIDDEN, "text/html; charset=utf-8");
            return true;
        }
    }

    return false;
}

bool QnUniversalRequestProcessor::redicrectToScheme(const char* scheme)
{
    Q_D(QnUniversalRequestProcessor);
    const auto schemeString = QString::fromUtf8(scheme);
    nx::utils::Url url(d->request.requestLine.url);
    if (url.scheme() == schemeString)
    {
        NX_ASSERT(false, lm("Unable to insecure connection on sheme: %1").arg(schemeString));
        d->response.messageBody = STATIC_BAD_REQUEST_HTML;
        sendResponse(CODE_FORBIDDEN, "text/html; charset=utf-8");
        return true;
    }

    const auto host = nx::network::http::getHeaderValue(d->request.headers, "Host");
    const auto endpoint(host.isEmpty()
        ? d->socket->getLocalAddress()
        : nx::network::SocketAddress(host));

    url.setHost(endpoint.address.toString());
    url.setPort(endpoint.port);
    url.setScheme(scheme);
    NX_VERBOSE(this, lm("Redirecting '%1' from '%2' to '%3'").args(
        d->request.requestLine.url, d->socket->getLocalAddress(), url));

    QByteArray contentType;
    int code = redirectTo(url.toString().toUtf8(), contentType);
    sendResponse(code, contentType);
    return true;
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
