#include "ec2_connection_processor.h"

#include <QtCore/QElapsedTimer>

#include <network/http_connection_listener.h>
#include <network/tcp_connection_priv.h>
#include <rest/server/rest_connection_processor.h>
#include <nx/network/app_info.h>
#include "appserver2_process.h"
#include <common/common_module.h>
#include <core/resource/user_resource.h>
#include <core/resource_management/resource_pool.h>

Ec2ConnectionProcessor::Ec2ConnectionProcessor(
    QSharedPointer<nx::network::AbstractStreamSocket> socket,
    QnHttpConnectionListener* owner)
:
    QnTCPConnectionProcessor(socket, owner)
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
        lit("Digest realm=\"%1\", nonce=\"%2\", algorithm=MD5")
        .arg(nx::network::AppInfo::realm())
        .arg(QDateTime::currentDateTime().toMSecsSinceEpoch());

    nx::network::http::insertOrReplaceHeader(&response.headers, nx::network::http::HttpHeader(
        "WWW-Authenticate",
        auth.toLatin1()));
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
        addAuthHeader(
            d->response);
        return false;
    }

    const auto findUserByName =
        [this](const QString& userName)
        {
            const auto userNameLower = userName.toLower();
            for (const auto& user: commonModule()->resourcePool()->getResources<QnUserResource>())
            {
                if (user->getName().toLower() == userNameLower)
                    return user;
            }
            return QnUserResourcePtr();
        };

    nx::network::http::header::Authorization authorizationHeader;
    if (authorizationHeader.parse(authorization))
    {
        const QByteArray userName = authorizationHeader.userid();
        if (auto userRes = findUserByName(QString::fromUtf8(userName)))
            d->accessRights = Qn::UserAccessData(userRes->getId());
    }

    return true;
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
                sendUnauthorizedResponse(nx::network::http::StatusCode::unauthorized, STATIC_UNAUTHORIZED_HTML);
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
        m_processor = handler(d->socket, owner);
    else
        return false;

    if (d->accessRights.userId.isNull())
        d->accessRights = Qn::kSystemAccess;

    auto restProcessor = dynamic_cast<QnRestConnectionProcessor*>(m_processor);
    if (restProcessor)
        restProcessor->setAuthNotRequired(noAuth);
    if (!needToStop())
    {
        copyClientRequestTo(*m_processor);
        if (m_processor->isSocketTaken())
            d->socket.clear(); // some of handlers have addition thread and depend of socket destructor. We should clear socket immediately to prevent race condition
        m_processor->execute(m_mutex);
        if (!m_processor->isSocketTaken())
            m_processor->releaseSocket();
        else
            d->socket.clear(); // some of handlers set ownership dynamically during a execute call. So, check it again.
    }

    delete m_processor;
    m_processor = nullptr;

    return true;
}

void Ec2ConnectionProcessor::pleaseStop()
{
    QnMutexLocker lk(&m_mutex);
    QnTCPConnectionProcessor::pleaseStop();
}
