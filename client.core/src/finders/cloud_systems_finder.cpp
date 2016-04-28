
#include "cloud_systems_finder.h"

#include <QtCore/QTimer>

#include <utils/common/delayed.h>
#include <utils/common/model_functions.h>
#include <nx/utils/raii_guard.h>
#include <nx/network/socket_global.h>
#include <nx/network/http/asynchttpclient.h>
#include <rest/server/json_rest_result.h>

namespace
{
    enum 
    {
        kCloudSystemsRefreshPeriodMs = 7 * 1000         // 7 seconds
        , kCloudServerOutdateTimeoutMs = 10 * 1000      // 10 seconds
    };

    constexpr static const std::chrono::seconds kSystemConnectTimeout = 
        std::chrono::seconds(15);
}

QnCloudSystemsFinder::QnCloudSystemsFinder(QObject *parent)
    : base_type(parent)
    , m_updateSystemsTimer(new QTimer(this))
    , m_mutex(QnMutex::Recursive)
    , m_systems()
    , m_requestToSystem()
{
    NX_ASSERT(qnCloudStatusWatcher, Q_FUNC_INFO, "Cloud watcher is not ready");

    connect(qnCloudStatusWatcher, &QnCloudStatusWatcher::statusChanged
        , this, &QnCloudSystemsFinder::onCloudStatusChanged);
    connect(qnCloudStatusWatcher, &QnCloudStatusWatcher::cloudSystemsChanged
        , this, &QnCloudSystemsFinder::setCloudSystems);
    connect(qnCloudStatusWatcher, &QnCloudStatusWatcher::error
        , this, &QnCloudSystemsFinder::onCloudError);

    connect(m_updateSystemsTimer, &QTimer::timeout
        , this, &QnCloudSystemsFinder::updateSystems);
    m_updateSystemsTimer->setInterval(kCloudSystemsRefreshPeriodMs);
    m_updateSystemsTimer->start();
}

QnCloudSystemsFinder::~QnCloudSystemsFinder()
{}

QnAbstractSystemsFinder::SystemDescriptionList QnCloudSystemsFinder::systems() const
{
    const QnMutexLocker lock(&m_mutex);
    return m_systems.values();
}

QnSystemDescriptionPtr QnCloudSystemsFinder::getSystem(const QString &id) const
{
    const QnMutexLocker lock(&m_mutex);

    const auto systemDescriptions = m_systems.values();
    const auto predicate = [id](const QnSystemDescriptionPtr &desc)
    {
        return (desc->id() == id);
    };

    const auto it = std::find_if(systemDescriptions.begin()
        , systemDescriptions.end(), predicate);

    return (it == systemDescriptions.end()
        ? QnSystemDescriptionPtr() : *it);
}


void QnCloudSystemsFinder::onCloudStatusChanged(QnCloudStatusWatcher::Status status)
{
    // TODO: #ynikitenkov Add handling of status changes
    switch (status)
    {
    case QnCloudStatusWatcher::Online:
    case QnCloudStatusWatcher::LoggedOut:
    case QnCloudStatusWatcher::Unauthorized:
    case QnCloudStatusWatcher::Offline:
        break;
    }
}

void QnCloudSystemsFinder::setCloudSystems(const QnCloudSystemList &systems)
{
    SystemsHash updatedSystems;
    for (const auto system : systems)
    {
        updatedSystems.insert(system.id
            , QnSystemDescription::createCloudSystem(system.id
                , system.name, system.ownerAccountEmail
                , system.ownerFullName));
    }

    typedef QSet<QString> IdsSet;

    const auto newIds = updatedSystems.keys().toSet();
    IdsSet removedIds;

    {
        const QnMutexLocker lock(&m_mutex);

        const auto oldIds = m_systems.keys().toSet();
        const auto addedIds = IdsSet(newIds).subtract(oldIds);

        removedIds = IdsSet(oldIds).subtract(newIds);

        std::swap(m_systems, updatedSystems);
        for (const auto system : m_systems)
        {
            if (addedIds.contains(system->id()))
            {
                updateSystemInternal(system);
                emit systemDiscovered(system);
            }
        }
    }

    for (const auto removedId : removedIds)
        emit systemLost(removedId);
}

void QnCloudSystemsFinder::onCloudError(QnCloudStatusWatcher::ErrorCode error)
{
    // TODO: #ynikitenkov handle errors. Now it is assumed that systems are reset
    // if any error automatically
}

void QnCloudSystemsFinder::updateSystemInternal(const QnSystemDescriptionPtr &system)
{
    using namespace nx::network;
    typedef std::vector<cloud::TypedAddress> AddressVector;

    auto &resolver = nx::network::SocketGlobals::addressResolver();
    const QPointer<QnCloudSystemsFinder> guard(this);
    const auto systemId = system->id();
    const auto resolvedHandler = [this, guard, systemId](const AddressVector &hosts)
    {
        if (!guard)
            return;

        {
            const QnMutexLocker lock(&m_mutex);
            const auto it = m_systems.find(systemId);
            if (it == m_systems.end())
                return;
        }

        for (const auto &host : hosts)
        {
            pingServerInternal(host.address.toString()
                , static_cast<int>(host.type), systemId);
        }
    };

    const auto cloudHost = HostAddress(systemId);
    resolver.resolveDomain(cloudHost, resolvedHandler);

    checkOutdatedServersInternal(system);
}

void QnCloudSystemsFinder::tryRemoveAlienServer(const QnModuleInformation &serverInfo
    , const QString &supposedSystemId)
{
    const auto serverId = serverInfo.id;
    for (const auto system : m_systems)
    {
        if ((system->id() != serverInfo.cloudSystemId)
            && system->containsServer(serverId))
        {
            system->removeServer(serverId);
        }
    }
}

void QnCloudSystemsFinder::pingServerInternal(const QString &host
    , int serverPriority
    , const QString &systemId)
{
    static const auto kModuleInformationTemplate
        = lit("http://%1:0/api/moduleInformation");
    const auto apiUrl = QUrl(kModuleInformationTemplate.arg(host));

    const QPointer<QnCloudSystemsFinder> guard(this);
    const auto onModuleInformationCompleted = [this, guard, systemId, host, serverPriority]
        (SystemError::ErrorCode errorCode, int httpCode, nx_http::BufferType buffer)
    {
        if (!guard || (errorCode != SystemError::noError)
            || (httpCode != nx_http::StatusCode::ok))
        {
            return;
        }

        QnJsonRestResult jsonReply;
        if (!QJson::deserialize(buffer, &jsonReply))
            return;

        QnModuleInformation moduleInformation;
        if (!QJson::deserialize(jsonReply.reply, &moduleInformation))
            return;

        const QnMutexLocker lock(&m_mutex);
        const auto it = m_systems.find(systemId);
        if (it == m_systems.end())
            return;

        // To prevent hanging on of fake online cloud servers
        // It is almost not hack.
        tryRemoveAlienServer(moduleInformation, systemId);
        if (systemId != moduleInformation.cloudSystemId)
            return;

        const auto serverId = moduleInformation.id;
        const auto systemDescription = it.value();
        if (systemDescription->containsServer(serverId))
            systemDescription->updateServer(moduleInformation);
        else
            systemDescription->addServer(moduleInformation, serverPriority);

        systemDescription->setServerHost(serverId, host);
    };

    nx_http::AsyncHttpClient::Timeouts httpRequestTimeouts;
    //first connect to a cloud (and not cloud) system may take a long time
    //  since it may require hole punching
    httpRequestTimeouts.sendTimeout = kSystemConnectTimeout;
    httpRequestTimeouts.responseReadTimeout = kSystemConnectTimeout;
    nx_http::downloadFileAsync(
        apiUrl,
        onModuleInformationCompleted,
        nx_http::HttpHeaders(),
        nx_http::AsyncHttpClient::authBasicAndDigest,
        httpRequestTimeouts);
}

void QnCloudSystemsFinder::checkOutdatedServersInternal(const QnSystemDescriptionPtr &system)
{
    const auto servers = system->servers();
    for (const auto serverInfo : servers)
    {
        const auto serverId = serverInfo.id;
        const auto elapsed = system->getServerLastUpdatedMs(serverId);
        if (elapsed > kCloudServerOutdateTimeoutMs)
            system->removeServer(serverId);
    }
}

void QnCloudSystemsFinder::updateSystems()
{
    const QnMutexLocker lock(&m_mutex);
    for (const auto system : m_systems.values())
        updateSystemInternal(system);
}
