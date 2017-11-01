
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
{
    const QnMutexLocker lock(&m_mutex);
    for (auto request: m_runningRequests)
        request->pleaseStopSync();
    m_runningRequests.clear();
}

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
        const auto systemDescription = QnCloudSystemDescription::create(
            targetId, system.localId, system.name, system.ownerAccountEmail,
            system.ownerFullName, system.online);
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

        updateOnlineStateUnsafe(systems);
    }

    for (const auto id: removedTargetIds.keys())
    {
        const auto localId = removedTargetIds[id];
        emit systemLostInternal(id, localId);
        emit systemLost(id);
    }
}

void QnCloudSystemsFinder::updateOnlineStateUnsafe(const QnCloudSystemList& targetSystems)
{
    for (const auto system: targetSystems)
    {
        const auto itCurrent = m_systems.find(system.cloudId);
        if (itCurrent != m_systems.end())
            itCurrent.value()->setRunning(system.online);
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
    client->setAuthType(nx_http::AuthType::authBasicAndDigest);
    // First connection to a system (cloud and not cloud) may take a long time
    // because it may require hole punching.
    client->setSendTimeoutMs(kSystemConnectTimeout.count());
    client->setResponseReadTimeoutMs(kSystemConnectTimeout.count());

    QPointer<QObject> guard(this);

    const auto handleReply =
        [this, guard, cloudSystemId](nx_http::AsyncHttpClientPtr reply)
        {
            const bool failed = reply->failed();
            const QByteArray data = failed ? QByteArray() : reply->fetchMessageBodyBuffer();

            NX_ASSERT(guard, "If we are already in the handler, destructor must wait on stopSync.");

            executeInThread(guard->thread(),
                [this, guard, cloudSystemId, failed, data, reply]
                {
                    if (!guard)
                        return;

                    const QnMutexLocker lock(&m_mutex);
                    m_runningRequests.removeOne(reply);

                    const auto it = m_systems.find(cloudSystemId);
                    if (it == m_systems.end())
                        return;

                    const auto systemDescription = it.value();
                    const auto clearServers =
                        [systemDescription]()
                        {
                            const auto currentServers = systemDescription->servers();
                            NX_ASSERT(currentServers.size() <= 1,
                                "There should be one or zero servers");
                            for (const auto& current : currentServers)
                                systemDescription->removeServer(current.id);
                        };

                    auto clearServersTask = QnRaiiGuard::createDestructible(clearServers);
                    if (failed)
                        return;

                    QnJsonRestResult jsonReply;
                    if (!QJson::deserialize(data, &jsonReply))
                        return;

                    QnModuleInformation moduleInformation;
                    if (!QJson::deserialize(jsonReply.reply, &moduleInformation))
                        return;

                    // To prevent hanging on of fake online cloud servers
                    // It is almost not hack.
                    tryRemoveAlienServer(moduleInformation);
                    if (cloudSystemId != moduleInformation.cloudSystemId)
                        return;

                    clearServersTask->disableDestructionHandler();

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

                    nx::utils::Url url;
                    url.setHost(moduleInformation.cloudId());
                    url.setScheme(moduleInformation.sslAllowed ? lit("https") : lit("http"));
                    systemDescription->setServerHost(serverId, url);
                }); //< executeInThread
            reply->pleaseStopSync();

        };

    client->doGet(lit("http://%1:0/api/moduleInformation").arg(cloudSystemId), handleReply);

    // This method is always called under mutex.
    m_runningRequests.push_back(client);
}

void QnCloudSystemsFinder::updateSystems()
{
    const QnMutexLocker lock(&m_mutex);
    for (auto it = m_systems.begin(); it != m_systems.end(); ++it)
        pingCloudSystem(it.key());
}
