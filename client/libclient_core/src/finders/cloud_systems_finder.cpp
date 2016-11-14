
#include "cloud_systems_finder.h"

#include <QtCore/QTimer>

#include <utils/common/delayed.h>
#include <nx/fusion/model_functions.h>
#include <nx/utils/raii_guard.h>
#include <nx/network/socket_global.h>
#include <nx/network/http/asynchttpclient.h>
#include <nx/network/http/async_http_client_reply.h>
#include <rest/server/json_rest_result.h>
#include <network/system_helpers.h>

static const std::chrono::milliseconds kCloudSystemsRefreshPeriod = std::chrono::seconds(7);
static const std::chrono::milliseconds kCloudServerOutdateTimeout = std::chrono::seconds(10);
static const std::chrono::milliseconds kSystemConnectTimeout = std::chrono::seconds(15);

QnCloudSystemsFinder::QnCloudSystemsFinder(QObject *parent)
    : base_type(parent)
    , m_updateSystemsTimer(new QTimer(this))
    , m_mutex(QnMutex::Recursive)
    , m_systems()
    , m_factorySystems()
    , m_requestToSystem()
{
    NX_ASSERT(qnCloudStatusWatcher, Q_FUNC_INFO, "Cloud watcher is not ready");

    connect(qnCloudStatusWatcher, &QnCloudStatusWatcher::statusChanged
        , this, &QnCloudSystemsFinder::onCloudStatusChanged);
    connect(qnCloudStatusWatcher, &QnCloudStatusWatcher::cloudSystemsChanged
        , this, &QnCloudSystemsFinder::setCloudSystems);

    connect(qnCloudStatusWatcher, &QnCloudStatusWatcher::recentCloudSystemsChanged, this
        , [this]()
        {
            if (qnCloudStatusWatcher->status() == QnCloudStatusWatcher::Offline)
                setCloudSystems(qnCloudStatusWatcher->recentCloudSystems());
        });

    connect(m_updateSystemsTimer, &QTimer::timeout
        , this, &QnCloudSystemsFinder::updateSystems);
    m_updateSystemsTimer->setInterval(kCloudSystemsRefreshPeriod.count());
    m_updateSystemsTimer->start();

    onCloudStatusChanged(qnCloudStatusWatcher->status());
}

QnCloudSystemsFinder::~QnCloudSystemsFinder()
{}

QnAbstractSystemsFinder::SystemDescriptionList QnCloudSystemsFinder::systems() const
{
    SystemDescriptionList result;

    const QnMutexLocker lock(&m_mutex);
    for (const auto& system : SystemsHash(m_systems).unite(m_factorySystems))
        result.append(system.dynamicCast<QnBaseSystemDescription>());

    return result;
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
        ? QnSystemDescriptionPtr() : (*it).dynamicCast<QnBaseSystemDescription>());
}


void QnCloudSystemsFinder::onCloudStatusChanged(QnCloudStatusWatcher::Status status)
{
    // TODO: #ynikitenkov Add handling of status changes
    if (status == QnCloudStatusWatcher::LoggedOut)
        setCloudSystems(QnCloudSystemList());
    else if (status == QnCloudStatusWatcher::Offline)
        setCloudSystems(qnCloudStatusWatcher->recentCloudSystems());
    else if (status == QnCloudStatusWatcher::Online)
        setCloudSystems(qnCloudStatusWatcher->cloudSystems());
}

void QnCloudSystemsFinder::setCloudSystems(const QnCloudSystemList &systems)
{
    SystemsHash updatedSystems;
    for (const auto system : systems)
    {
        NX_ASSERT(!helpers::isNewSystem(system), "Cloud system can't be NEW system");

        const auto targetId = helpers::getTargetSystemId(system);
        const auto systemDescription = QnSystemDescription::createCloudSystem(targetId,
            system.localId, system.name, system.ownerAccountEmail, system.ownerFullName);
        updatedSystems.insert(system.cloudId, systemDescription);
    }

    typedef QSet<QString> IdsSet;

    const auto newIds = updatedSystems.keys().toSet();

    QHash<QString, QnUuid> removedTargetIds;

    {
        const QnMutexLocker lock(&m_mutex);

        const auto oldIds = m_systems.keys().toSet();
        const auto addedCloudIds = IdsSet(newIds).subtract(oldIds);

        const auto removedCloudIds = IdsSet(oldIds).subtract(newIds);
        for (const auto addedCloudId : addedCloudIds)
        {
            const auto system = updatedSystems[addedCloudId];
            emit systemDiscovered(system);

            m_systems.insert(addedCloudId, system);
            updateSystemInternal(addedCloudId, system);
        }

        for (const auto removedCloudId : removedCloudIds)
        {
            const auto system = m_systems[removedCloudId];
            removedTargetIds.insert(system->id(), system->localId());
            m_systems.remove(removedCloudId);
        }
    }

    for (const auto id: removedTargetIds.keys())
    {
        const auto localId = removedTargetIds[id];
        emit systemLostInternal(id, localId);
        emit systemLost(id);
    }
}

void QnCloudSystemsFinder::updateSystemInternal(
    const QString& cloudId,
    const QnSystemDescription::PointerType& system)
{
    using namespace nx::network;
    typedef std::vector<cloud::TypedAddress> AddressVector;

    auto &resolver = nx::network::SocketGlobals::addressResolver();
    const QPointer<QnCloudSystemsFinder> guard(this);
    const auto resolvedHandler =
        [this, guard, cloudId](const AddressVector &hosts)
        {
            if (!guard)
                return;

            {
                const QnMutexLocker lock(&m_mutex);
                const auto it = m_systems.find(cloudId);
                if (it == m_systems.end())
                    return;
            }

            for (const auto &host : hosts)
            {
                pingServerInternal(host.address.toString(),
                    static_cast<int>(host.type), cloudId);
            }
        };

    const auto cloudHost = HostAddress(cloudId);
    resolver.resolveDomain(cloudHost, resolvedHandler);

    checkOutdatedServersInternal(system);
}

void QnCloudSystemsFinder::tryRemoveAlienServer(const QnModuleInformation &serverInfo)
{
    const auto serverId = serverInfo.id;
    for (auto it = m_systems.begin(); it != m_systems.end(); ++it)
    {
        const auto cloudSystemId = it.key();
        const bool remove = (cloudSystemId != serverInfo.cloudSystemId);
        const auto system = it.value();
        if (remove && system->containsServer(serverId))
            system->removeServer(serverId);
    }
}

void QnCloudSystemsFinder::pingServerInternal(
    const QString& host,
    int serverPriority,
    const QString& systemId)
{
    auto client = nx_http::AsyncHttpClient::create();
    client->setAuthType(nx_http::AsyncHttpClient::authBasicAndDigest);
    // First connection to a system (cloud and not cloud) may take a long time
    // because it may require hole punching.
    client->setSendTimeoutMs(kSystemConnectTimeout.count());
    client->setResponseReadTimeoutMs(kSystemConnectTimeout.count());

    typedef QSharedPointer<QnAsyncHttpClientReply> ReplyPtr;
    auto replyHolder = ReplyPtr(new QnAsyncHttpClientReply(client));

    const auto handleReply =
        [this, systemId, host, serverPriority, replyHolder]
            (QnAsyncHttpClientReply* reply) mutable
        {
            /**
             * Forces "manual" deletion instead of "deleteLater" because we don't
             * have event loop in this thread.
            **/
            const auto replyDeleter = QnRaiiGuard::createDestructable(
                [&replyHolder]() { replyHolder.reset(); });

            if (reply->isFailed())
                return;

            QnJsonRestResult jsonReply;
            if (!QJson::deserialize(reply->data(), &jsonReply))
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
            tryRemoveAlienServer(moduleInformation);

            if (systemId != moduleInformation.cloudSystemId)
                return;

            const auto serverId = moduleInformation.id;
            const auto systemDescription = it.value();
            if (systemDescription->containsServer(serverId))
                systemDescription->updateServer(moduleInformation);
            else
                systemDescription->addServer(moduleInformation, serverPriority);

            QUrl url;
            url.setHost(host);
            systemDescription->setServerHost(serverId, url);
        };

    connect(replyHolder, &QnAsyncHttpClientReply::finished, this, handleReply);
    client->doGet(lit("http://%1:0/api/moduleInformation").arg(host));
}

void QnCloudSystemsFinder::checkOutdatedServersInternal(
    const QnSystemDescription::PointerType &system)
{
    const auto servers = system->servers();
    for (const auto serverInfo : servers)
    {
        const auto serverId = serverInfo.id;
        const auto elapsed = system->getServerLastUpdatedMs(serverId);
        if (elapsed > kCloudServerOutdateTimeout.count())
        {
            system->removeServer(serverId);

            // Removes factory systems. Note: factory id is just id of server
            if (system->servers().isEmpty() && m_factorySystems.remove(system->id()))
            {
                const auto id = system->id();
                emit systemLostInternal(id, system->localId());
                emit systemLost(id);
            }
        }
    }

}

void QnCloudSystemsFinder::updateSystems()
{
    const QnMutexLocker lock(&m_mutex);
    const auto targetSystems = SystemsHash(m_systems).unite(m_factorySystems);
    for (auto it = targetSystems.begin(); it != targetSystems.end(); ++it)
    {
        const auto cloudId = it.key();
        const auto system = it.value();
        updateSystemInternal(cloudId, system);
    }
}
