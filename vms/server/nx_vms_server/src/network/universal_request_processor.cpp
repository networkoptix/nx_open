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
#include <nx/vms/network/proxy_connection.h>

namespace {

static const int AUTH_TIMEOUT = 60 * 1000;
static const int MAX_AUTH_RETRY_COUNT = 3;


} // namespace

QnUniversalRequestProcessor::~QnUniversalRequestProcessor()
{
    stop();
}

QnUniversalRequestProcessor::QnUniversalRequestProcessor(
    std::unique_ptr<nx::network::AbstractStreamSocket> socket,
    QnUniversalTcpListener* owner, bool needAuth)
:
    QnTCPConnectionProcessor(new QnUniversalRequestProcessorPrivate, std::move(socket), owner)
{
    Q_D(QnUniversalRequestProcessor);
    d->listener = owner;
    d->processor = 0;
    d->needAuth = needAuth;

    setObjectName( QLatin1String("QnUniversalRequestProcessor") );
}

QnUniversalRequestProcessor::QnUniversalRequestProcessor(
    QnUniversalRequestProcessorPrivate* priv,
    std::unique_ptr<nx::network::AbstractStreamSocket> socket,
    QnUniversalTcpListener* owner,
    bool needAuth)
:
    QnTCPConnectionProcessor(priv, std::move(socket), owner)
{
    Q_D(QnUniversalRequestProcessor);
    d->processor = 0;
    d->needAuth = needAuth;
}

static QByteArray m_unauthorizedPageBody;
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
        const bool isProxy = nx::vms::network::ProxyConnectionProcessor::isStandardProxyNeeded(
            d->owner->commonModule(), d->request);
        QElapsedTimer t;
        t.restart();
        QnAuthSession lastUnauthorizedData;
        const auto clientIp = d->socket->getForeignAddress().address;
        nx::vms::server::Authenticator::Result result;
        while ((result = d->listener->authenticator()->tryAllMethods(
            clientIp, d->request, &d->response, isProxy)).code != Qn::Auth_OK)
        {
            const auto resultReason = toString(result.code).toUtf8();
            lastUnauthorizedData = authSession();
            nx::network::http::insertOrReplaceHeader(
                &d->response.headers, {Qn::AUTH_RESULT_HEADER_NAME, resultReason});

            if (!d->socket->isConnected())
                break;

            nx::network::http::StatusCode::Value httpResult;
            QByteArray msgBody;
            if (isProxy)
            {
                httpResult = nx::network::http::StatusCode::proxyAuthenticationRequired;
            }
            else if (result.code ==  Qn::Auth_Forbidden)
            {
                httpResult = nx::network::http::StatusCode::forbidden;
            }
            else
            {
                httpResult = nx::network::http::StatusCode::unauthorized;
                if (result.usedMethods & m_unauthorizedPageForMethods)
                    msgBody = unauthorizedPageBody();
            }
            sendUnauthorizedResponse(httpResult, msgBody, resultReason);

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
                auto auditManager = d->owner->commonModule()->auditManager();
                auditManager->addAuditRecord(auditManager->prepareRecord(
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
    if (!isConnectionSecure())
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
                return redicrectToScheme(nx::network::rtsp::kSecureUrlSchemeName);
        }
        else if (settings->isTrafficEncriptionForced())
        {
            NX_ASSERT(false, lm("Unable to redirect protocol to secure version: %1").arg(protocol));
            sendErrorResponse(nx::network::http::StatusCode::forbidden);
            return true;
        }
    }

    return false;
}

bool QnUniversalRequestProcessor::redicrectToScheme(const char* scheme)
{
    const auto schemeString = QString::fromUtf8(scheme);
    Q_D(QnUniversalRequestProcessor);

    const auto listener = dynamic_cast<QnHttpConnectionListener*>(d->owner);
    if (listener && listener->isProxy(d->request) && d->request.headers.count("Via"))
    {
        NX_ASSERT(false, lm("Unable to redirect sheme %1 for proxy").arg(schemeString));
        sendErrorResponse(nx::network::http::StatusCode::forbidden);
        return true;
    }

    nx::utils::Url url(d->request.requestLine.url);
    if (url.scheme() == schemeString)
    {
        NX_ASSERT(false, lm("Unable to insecure connection on sheme: %1").arg(schemeString));
        sendErrorResponse(nx::network::http::StatusCode::forbidden);
        return true;
    }

    const auto host = nx::network::http::getHeaderValue(d->request.headers, "Host");
    const auto endpoint(host.isEmpty()
        ? d->socket->getLocalAddress()
        : nx::network::SocketAddress(host));

    url.setHost(endpoint.address.toString());
    if (endpoint.port > 0)
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
        d->processor = handler(std::move(d->socket), owner);
    else
        return false;

    auto restProcessor = dynamic_cast<QnRestConnectionProcessor*>(d->processor);
    if (restProcessor)
        restProcessor->setAuthNotRequired(noAuth);
    if ( !needToStop() )
    {
        copyClientRequestTo(*d->processor);
        d->processor->execute(lock);
        // Get socket back(if still exists) for the next request.
        d->socket = d->processor->takeSocket();
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
