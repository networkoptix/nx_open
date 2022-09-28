// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "cloud_systems_finder.h"

#include <QtCore/QTimer>

#include <network/system_helpers.h>
#include <nx/branding.h>
#include <nx/build_info.h>
#include <nx/network/cloud/cloud_connect_controller.h>
#include <nx/network/deprecated/asynchttpclient.h>
#include <nx/network/http/async_http_client_reply.h>
#include <nx/network/rest/result.h>
#include <nx/network/socket_global.h>
#include <nx/utils/log/log.h>
#include <nx/utils/scope_guard.h>
#include <nx/vms/api/protocol_version.h>
#include <nx/vms/client/core/ini.h>
#include <utils/common/delayed.h>

static const std::chrono::milliseconds kCloudSystemsRefreshPeriod = std::chrono::seconds(15);
static const std::chrono::milliseconds kSystemConnectTimeout = std::chrono::seconds(12);

namespace {

nx::utils::Url systemUrl(const QString& cloudSystemId)
{
    nx::utils::Url url;
    url.setScheme(nx::network::http::kSecureUrlSchemeName);
    url.setHost(cloudSystemId);
    return url;
}

nx::utils::Url makeCloudModuleInformationUrl(const QString& cloudSystemId)
{
    nx::utils::Url url = systemUrl(cloudSystemId);
    url.setPath("/api/moduleInformation");
    return url;
}

/**
 * By default if cloud system is listed as online we create some initial server record, so the
 * system is displayed as reachable. If ping request fails, server will be removed.
 */
nx::vms::api::ModuleInformationWithAddresses createInitialServer(const QnCloudSystem& system)
{
    nx::vms::api::ModuleInformationWithAddresses result;

    // Fill parameters with default values to avoid compatibility checks.
    //result.id = kInitialServerId;
    result.type = nx::vms::api::ModuleInformation::mediaServerId();
    result.brand = nx::branding::brand();
    result.customization = nx::branding::customization();
    result.cloudHost = QString::fromStdString(nx::network::SocketGlobals::cloud().cloudHost());
    result.sslAllowed = true;

    result.systemName = system.name;
    result.cloudSystemId = system.cloudId;
    result.localSystemId = system.localId;
    result.remoteAddresses.insert(system.cloudId);
    result.version = nx::vms::api::SoftwareVersion(system.version);

    const auto localVersion = nx::vms::api::SoftwareVersion(nx::build_info::vmsVersion());
    const bool sameVersion = result.version.major() == localVersion.major()
        && result.version.minor() == localVersion.minor();

    result.protoVersion = sameVersion ? nx::vms::api::protocolVersion() : 0;

    return result;
}

} // namespace

QnCloudSystemsFinder::QnCloudSystemsFinder(QObject* parent):
    base_type(parent),
    m_updateSystemsTimer(new QTimer(this)),
    m_mutex(nx::Mutex::Recursive),
    m_systems()
{
    NX_ASSERT(qnCloudStatusWatcher, "Cloud watcher is not ready");

    connect(qnCloudStatusWatcher,
        &QnCloudStatusWatcher::statusChanged,
        this,
        &QnCloudSystemsFinder::onCloudStatusChanged);
    connect(qnCloudStatusWatcher,
        &QnCloudStatusWatcher::cloudSystemsChanged,
        this,
        &QnCloudSystemsFinder::setCloudSystems);

    connect(m_updateSystemsTimer, &QTimer::timeout, this, &QnCloudSystemsFinder::updateSystems);
    m_updateSystemsTimer->setInterval(kCloudSystemsRefreshPeriod.count());
    m_updateSystemsTimer->start();

    onCloudStatusChanged(qnCloudStatusWatcher->status());
}

QnCloudSystemsFinder::~QnCloudSystemsFinder()
{
    const NX_MUTEX_LOCKER lock(&m_mutex);
    for (auto request: m_runningRequests)
        request->pleaseStopSync();
    m_runningRequests.clear();
}

QnAbstractSystemsFinder::SystemDescriptionList QnCloudSystemsFinder::systems() const
{
    SystemDescriptionList result;

    const NX_MUTEX_LOCKER lock(&m_mutex);
    for (const auto& system : m_systems)
        result.append(system.dynamicCast<QnBaseSystemDescription>());

    return result;
}

QnSystemDescriptionPtr QnCloudSystemsFinder::getSystem(const QString &id) const
{
    const NX_MUTEX_LOCKER lock(&m_mutex);

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
    setCloudSystems(status == QnCloudStatusWatcher::Online
        ? qnCloudStatusWatcher->cloudSystems()
        : QnCloudSystemList());
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
            system.ownerFullName, system.online, system.system2faEnabled);

        if (system.online)
        {
            auto initialServer = createInitialServer(system);
            systemDescription->addServer(initialServer, QnSystemDescription::kDefaultPriority);
            systemDescription->setServerHost(initialServer.id, systemUrl(system.cloudId));
        }

        updatedSystems.insert(system.cloudId, systemDescription);
    }

    typedef QSet<QString> IdsSet;

    const auto newIds = updatedSystems.keys().toSet();

    QHash<QString, QnUuid> removedTargetIds;

    {
        const NX_MUTEX_LOCKER lock(&m_mutex);

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

        updateStateUnsafe(systems);
    }

    for (const auto id: removedTargetIds.keys())
    {
        const auto localId = removedTargetIds[id];
        emit systemLostInternal(id, localId);
        emit systemLost(id);
    }
}

void QnCloudSystemsFinder::updateStateUnsafe(const QnCloudSystemList& targetSystems)
{
    for (const auto system: targetSystems)
    {
        const auto itCurrent = m_systems.find(system.cloudId);
        if (itCurrent == m_systems.end())
            continue;

        const auto systemDescription = itCurrent.value();
        NX_DEBUG(this, "Set online state for the system \"%1\" <%2> to [%3]",
            systemDescription->name(), systemDescription->id(), system.online);
        systemDescription->setOnline(system.online);

        NX_DEBUG(this, "Set 2fa state for the system \"%1\" <%2> to [%3]",
            systemDescription->name(), systemDescription->id(), system.system2faEnabled);
        systemDescription->set2faEnabled(system.system2faEnabled);
    }
}

void QnCloudSystemsFinder::tryRemoveAlienServer(const nx::vms::api::ModuleInformation& serverInfo)
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
    if (nx::vms::client::core::ini().doNotPingCloudSystems)
        return;

    auto client = nx::network::http::AsyncHttpClient::create(nx::network::ssl::kAcceptAnyCertificate);
    client->setAuthType(nx::network::http::AuthType::authBasicAndDigest);
    // First connection to a system (cloud and not cloud) may take a long time
    // because it may require hole punching.
    client->setSendTimeoutMs(kSystemConnectTimeout.count());
    client->setResponseReadTimeoutMs(kSystemConnectTimeout.count());

    QPointer<QObject> guard(this);

    const auto handleReply =
        [this, guard, cloudSystemId](nx::network::http::AsyncHttpClientPtr reply)
        {
            const bool failed = reply->failed();
            const auto data = failed ? nx::Buffer() : reply->fetchMessageBodyBuffer();

            NX_ASSERT(guard, "If we are already in the handler, destructor must wait on stopSync.");

            executeInThread(guard->thread(),
                [this, guard, cloudSystemId, failed, data, reply]
                {
                    if (!guard)
                        return;

                    NX_VERBOSE(
                        this,
                        "Cloud system <%1>: ping request reply:\nSuccess: %2\n%3\n%4",
                        cloudSystemId,
                        !failed,
                        reply->response() ? reply->response()->toString() : "none",
                        data);

                    const NX_MUTEX_LOCKER lock(&m_mutex);
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

                    auto clearServersTask = nx::utils::makeScopeGuard(clearServers);
                    if (failed)
                        return;

                    nx::network::rest::JsonResult jsonReply;
                    if (!QJson::deserialize(data, &jsonReply))
                    {
                        NX_DEBUG(this, nx::format("Cloud system <%1>: failed to deserialize json reply:\n%2"),
                            cloudSystemId, data);
                        return;
                    }

                    nx::vms::api::ModuleInformationWithAddresses moduleInformation;
                    if (!QJson::deserialize(jsonReply.reply, &moduleInformation))
                    {
                        NX_DEBUG(this, nx::format("Cloud system <%1>: failed to deserialize module information:\n%2"),
                            cloudSystemId, data);
                        return;
                    }

                    // Prevent hanging of fake online cloud servers.
                    tryRemoveAlienServer(moduleInformation);
                    if (cloudSystemId != moduleInformation.cloudSystemId)
                        return;

                    clearServersTask.disarm();

                    const auto serverId = moduleInformation.id;
                    if (systemDescription->containsServer(serverId))
                    {
                        systemDescription->updateServer(moduleInformation);
                    }
                    else
                    {
                        // First we must add newly found server to make sure system will not become
                        // unreachable in the process.
                        systemDescription->addServer(moduleInformation,
                            QnSystemDescription::kDefaultPriority);
                        const auto currentServers = systemDescription->servers();
                        for (const auto& server: currentServers)
                        {
                            if (server.id != serverId)
                                systemDescription->removeServer(server.id);
                        }
                    }

                    nx::utils::Url url;
                    url.setHost(moduleInformation.cloudId());
                    url.setScheme(nx::network::http::urlScheme(moduleInformation.sslAllowed));
                    systemDescription->setServerHost(serverId, url);
                    NX_DEBUG(this, nx::format("Cloud system <%1>: set server <%2> url to %3"),
                        cloudSystemId, serverId.toString(), url.toString(QUrl::RemovePassword));
                }); //< executeInThread
            reply->pleaseStopSync();

        };

    NX_DEBUG(this, nx::format("Cloud system <%1>: send moduleInformation request"), cloudSystemId);
    client->doGet(makeCloudModuleInformationUrl(cloudSystemId), handleReply);

    // This method is always called under mutex.
    m_runningRequests.push_back(client);
}

void QnCloudSystemsFinder::updateSystems()
{
    const NX_MUTEX_LOCKER lock(&m_mutex);
    for (auto it = m_systems.begin(); it != m_systems.end(); ++it)
        pingCloudSystem(it.key());
}
