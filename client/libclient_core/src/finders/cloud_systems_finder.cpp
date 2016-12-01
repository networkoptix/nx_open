
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

static const std::chrono::milliseconds kCloudSystemsRefreshPeriod = std::chrono::seconds(15);
static const std::chrono::milliseconds kSystemConnectTimeout = std::chrono::seconds(12);

QnCloudSystemsFinder::QnCloudSystemsFinder(QObject *parent)
    : base_type(parent)
    , m_updateSystemsTimer(new QTimer(this))
    , m_mutex(QnMutex::Recursive)
    , m_systems()
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
    for (const auto& system : m_systems)
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
            pingCloudSystem(addedCloudId);
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

void QnCloudSystemsFinder::pingCloudSystem(const QString& cloudSystemId)
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
        [this, cloudSystemId, replyHolder]
            (QnAsyncHttpClientReply* reply) mutable
        {
            /**
             * Forces "manual" deletion instead of "deleteLater" because we don't
             * have event loop in this thread.
            **/
            const auto replyDeleter = QnRaiiGuard::createDestructable(
                [&replyHolder]() { replyHolder.reset(); });

            const QnMutexLocker lock(&m_mutex);
            const auto it = m_systems.find(cloudSystemId);
            if (it == m_systems.end())
                return;

            const auto systemDescription = it.value();
            const auto clearServers =
                [systemDescription]()
                {
                    const auto currentServers = systemDescription->servers();
                    NX_ASSERT(currentServers.size() <= 1, "There should be one or zero servers");
                    for (const auto& current : currentServers)
                        systemDescription->removeServer(current.id);
                };

            auto clearServersTask = QnRaiiGuard::createDestructable(clearServers);
            if (reply->isFailed())
                return;

            QnJsonRestResult jsonReply;
            if (!QJson::deserialize(reply->data(), &jsonReply))
                return;

            QnModuleInformation moduleInformation;
            if (!QJson::deserialize(jsonReply.reply, &moduleInformation))
                return;


            // To prevent hanging on of fake online cloud servers
            // It is almost not hack.
            tryRemoveAlienServer(moduleInformation);
            if (cloudSystemId != moduleInformation.cloudSystemId)
                return;

            clearServersTask = QnRaiiGuardPtr();

            const auto serverId = moduleInformation.id;
            if (systemDescription->containsServer(serverId))
            {
                systemDescription->updateServer(moduleInformation);
            }
            else
            {
                clearServers();
                systemDescription->addServer(moduleInformation, 0);
            }


            QUrl url;
            url.setHost(moduleInformation.cloudId());
            systemDescription->setServerHost(serverId, url);
        };

    connect(replyHolder, &QnAsyncHttpClientReply::finished, this, handleReply);
    client->doGet(lit("http://%1:0/api/moduleInformation").arg(cloudSystemId));
}

void QnCloudSystemsFinder::updateSystems()
{
    const QnMutexLocker lock(&m_mutex);
    for (auto it = m_systems.begin(); it != m_systems.end(); ++it)
        pingCloudSystem(it.key());
}
