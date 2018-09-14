#include "ec2_connection_processor.h"

#include <QtCore/QElapsedTimer>

#include <network/http_connection_listener.h>
#include <network/tcp_connection_priv.h>
#include <rest/server/rest_connection_processor.h>
#include <nx/network/app_info.h>
#include "appserver2_process.h"
#include <common/common_module.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/network/http/custom_headers.h>

Ec2ConnectionProcessor::Ec2ConnectionProcessor(
    std::unique_ptr<nx::network::AbstractStreamSocket> socket,
    nx::vms::auth::AbstractUserDataProvider* userDataProvider,
    nx::vms::auth::AbstractNonceProvider* nonceProvider,
    QnHttpConnectionListener* owner)
:
    QnTCPConnectionProcessor(std::move(socket), owner),
    m_userDataProvider(userDataProvider),
    m_nonceProvider(nonceProvider)
{
    setObjectName(QLatin1String("Ec2ConnectionProcessor"));
}

Ec2ConnectionProcessor::~Ec2ConnectionProcessor()
{
    stop();
}

void Ec2ConnectionProcessor::addAuthHeader(nx::network::http::Response& response)
{
    const QString auth =
        lm("Digest realm=\"%1\", nonce=\"%2\", algorithm=MD5")
            .args(nx::network::AppInfo::realm(), m_nonceProvider->generateNonce()).toQString();

    nx::network::http::insertOrReplaceHeader(
        &response.headers,
        nx::network::http::HttpHeader("WWW-Authenticate", auth.toLatin1()));
}

bool Ec2ConnectionProcessor::authenticate()
{
    Q_D(QnTCPConnectionProcessor);

    auto owner = static_cast<ec2::QnSimpleHttpConnectionListener*>(d->owner);
    if (!owner->needAuth(d->request))
        return true;

    const nx::network::http::StringType& authorization =
        nx::network::http::getHeaderValue(d->request.headers, "Authorization");
    if (authorization.isEmpty())
    {
        addAuthHeader(d->response);
        return false;
    }

    nx::network::http::header::Authorization authorizationHeader;
    if (!authorizationHeader.parse(authorization))
    {
        addAuthHeader(d->response);
        return false;
    }

    const auto authResult = m_userDataProvider->authorize(
        d->request.requestLine.method,
        authorizationHeader,
        &d->response.headers);
    if (auto authenticatedResource = std::get<1>(authResult))
    {
        if (auto userResource = authenticatedResource.dynamicCast<QnUserResource>())
            d->accessRights = Qn::UserAccessData(userResource->getId());
        else if (auto serverResource = authenticatedResource.dynamicCast<QnMediaServerResource>())
            d->accessRights = Qn::kSystemAccess;
    }
    return std::get<0>(authResult) == Qn::AuthResult::Auth_OK;
}

void Ec2ConnectionProcessor::run()
{
    Q_D(QnTCPConnectionProcessor);

    initSystemThreadId();

    if (!readRequest())
        return;

    QElapsedTimer t;
    t.restart();
    bool ready = true;
    bool isKeepAlive = false;

    int authenticateTries = 0;
    while (authenticateTries < 3)
    {
        if (ready)
        {
            t.restart();
            parseRequest();

            if (!authenticate())
            {
                sendUnauthorizedResponse(nx::network::http::StatusCode::unauthorized);
                ready = readRequest();
                ++authenticateTries;
                continue;
            }
            authenticateTries = 0;

            isKeepAlive = isConnectionCanBePersistent();


            // getting a new handler inside is necessary due to possibility of
            // changing request during authentication
            bool noAuth = false;
            if (!processRequest(noAuth))
            {
                QByteArray contentType;
                sendResponse(nx::network::http::StatusCode::badRequest, contentType);
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

bool Ec2ConnectionProcessor::processRequest(bool noAuth)
{
    Q_D(QnTCPConnectionProcessor);

    QnMutexLocker lock(&m_mutex);
    auto owner = static_cast<QnHttpConnectionListener*> (d->owner);
    if (auto handler = owner->findHandler(d->protocol, d->request))
        m_processor = handler(std::move(d->socket), owner);
    else
        return false;

    if (d->accessRights.userId.isNull())
    {
        auto nxUserName =
            nx::network::http::getHeaderValue(d->request.headers, Qn::CUSTOM_USERNAME_HEADER_NAME);
        const bool isSystemRequest = !QnUuid::fromStringSafe(nxUserName).isNull();
        if (isSystemRequest)
            d->accessRights = Qn::kSystemAccess;
        else
            d->accessRights.userId = QnUserResource::kAdminGuid;
    }

    auto restProcessor = dynamic_cast<QnRestConnectionProcessor*>(m_processor);
    if (restProcessor)
        restProcessor->setAuthNotRequired(noAuth);
    if (!needToStop())
    {
        copyClientRequestTo(*m_processor);
        m_processor->execute(lock);
        // Get socket back(if still exists) for the next request.
        d->socket = m_processor->takeSocket();
    }

    delete m_processor;
    m_processor = nullptr;

    return true;
}

void Ec2ConnectionProcessor::pleaseStop()
{
    QnMutexLocker lk(&m_mutex);
    if (m_processor)
        m_processor->pleaseStop();
    QnTCPConnectionProcessor::pleaseStop();
}
