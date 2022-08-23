// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "cloud_systems_finder.h"

#include <QtCore/QPointer>
#include <QtCore/QTimer>

#include <network/system_helpers.h>
#include <nx/network/deprecated/asynchttpclient.h>
#include <nx/network/http/async_http_client_reply.h>
#include <nx/network/rest/result.h>
#include <nx/network/socket_global.h>
#include <nx/utils/log/log.h>
#include <nx/utils/qset.h>
#include <nx/utils/scope_guard.h>
#include <nx/vms/client/core/application_context.h>
#include <utils/common/delayed.h>

static const std::chrono::milliseconds kCloudSystemsRefreshPeriod = std::chrono::seconds(15);
static const std::chrono::milliseconds kSystemConnectTimeout = std::chrono::seconds(12);

using namespace nx::vms::client::core;

namespace {

nx::utils::Url makeCloudModuleInformationUrl(const QString& cloudSystemId)
{
    nx::utils::Url url;
    url.setScheme(nx::network::http::kSecureUrlSchemeName);
    url.setHost(cloudSystemId);
    url.setPort(0);
    url.setPath("/api/moduleInformation");
    return url;
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
        &CloudStatusWatcher::statusChanged,
        this,
        &QnCloudSystemsFinder::onCloudStatusChanged);
    connect(qnCloudStatusWatcher,
        &CloudStatusWatcher::cloudSystemsChanged,
        this,
        &QnCloudSystemsFinder::setCloudSystems);

    connect(m_updateSystemsTimer.get(), &QTimer::timeout, this, &QnCloudSystemsFinder::updateSystems);
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


void QnCloudSystemsFinder::onCloudStatusChanged(CloudStatusWatcher::Status status)
{
    setCloudSystems(status == CloudStatusWatcher::Online
        ? qnCloudStatusWatcher->cloudSystems()
        : QnCloudSystemList());
}

void QnCloudSystemsFinder::setCloudSystems(const QnCloudSystemList &systems)
{
    SystemsHash updatedSystems;
    for (const auto& system: systems)
    {
        NX_ASSERT(!helpers::isNewSystem(system), "Cloud system can't be NEW system");

        const auto targetId = helpers::getTargetSystemId(system);
        const auto systemDescription = QnCloudSystemDescription::create(
            targetId, system.localId, system.name, system.ownerAccountEmail,
            system.ownerFullName, system.online, system.system2faEnabled);
        updatedSystems.insert(system.cloudId, systemDescription);
    }

    typedef QSet<QString> IdsSet;

    const auto newIds = nx::utils::toQSet(updatedSystems.keys());

    QHash<QString, QnUuid> removedTargetIds;

    {
        const NX_MUTEX_LOCKER lock(&m_mutex);

        const auto oldIds = nx::utils::toQSet(m_systems.keys());
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

        NX_DEBUG(this, "Update last known system version for the system \"%1\" <%2> to [%3]",
            systemDescription->name(), systemDescription->id(), system.newestServerVersion);
        const auto version = nx::utils::SoftwareVersion(system.newestServerVersion);
        systemDescription->updateLastKnownVersion(version);

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
                        clearServers();
                        systemDescription->addServer(moduleInformation, 0);
                    }

                    nx::utils::Url url;
                    url.setHost(moduleInformation.cloudId());
                    url.setScheme(nx::network::http::kSecureUrlSchemeName);
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
