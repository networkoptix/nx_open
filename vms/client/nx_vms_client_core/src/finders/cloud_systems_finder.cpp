// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "cloud_systems_finder.h"

#include <QtCore/QPointer>
#include <QtCore/QTimer>

#include <api/http_client_pool.h>
#include <network/system_helpers.h>
#include <nx/network/rest/result.h>
#include <nx/network/socket_global.h>
#include <nx/network/url/url_builder.h>
#include <nx/utils/async_handler_executor.h>
#include <nx/utils/log/log.h>
#include <nx/utils/qset.h>
#include <watchers/cloud_status_watcher.h>

using namespace std::chrono;
using nx::network::http::ClientPool;

namespace nx::vms::client::core {

namespace {

static constexpr milliseconds kCloudSystemsRefreshPeriod = 1min;

// First connection to a system (cloud and not cloud) may take a long time because it may require
// hole punching.
static constexpr nx::network::http::AsyncClient::Timeouts kDefaultTimeouts{
    .sendTimeout = 1min,
    .responseReadTimeout = 1min,
    .messageBodyReadTimeout = 1min
};

nx::utils::Url makeCloudModuleInformationUrl(const QString& cloudSystemId)
{
    return nx::network::url::Builder()
        .setScheme(nx::network::http::kSecureUrlSchemeName)
        .setHost(cloudSystemId)
        .setPort(0)
        .setPath("/api/moduleInformation")
        .toUrl();
}

} // namespace

struct CloudSystemsFinder::Private
{
    CloudSystemsFinder* const q;
    mutable nx::Mutex mutex{nx::Mutex::Recursive};
    std::unique_ptr<ClientPool> clientPool = std::make_unique<ClientPool>();
    using SystemsHash = QHash<QString, QnCloudSystemDescriptionPtr>;
    SystemsHash systems;

    void pingAllSystems()
    {
        NX_MUTEX_LOCKER lock(&mutex);
        for (auto it = systems.begin(); it != systems.end(); ++it)
            pingSystem(it.key());
    }

    void pingSystem(const QString& cloudSystemId)
    {
        static const QnUuid kAdapterFuncId = QnUuid::createUuid();
        NX_DEBUG(this, "Cloud system <%1>: send moduleInformation request", cloudSystemId);

        ClientPool::Request request;
        request.method = nx::network::http::Method::get;
        request.url = makeCloudModuleInformationUrl(cloudSystemId);

        ClientPool::ContextPtr context(new ClientPool::Context(
            kAdapterFuncId,
            nx::network::ssl::kAcceptAnyCertificate));
        context->request = request;
        context->completionFunc = nx::utils::AsyncHandlerExecutor(q).bind(
            [this, cloudSystemId](ClientPool::ContextPtr context)
            {
                onRequestComplete(cloudSystemId, context);
            });
        context->setTargetThread(q->thread());
        clientPool->sendRequest(context);
    }

    bool tryRemoveAlienServerUnderLock(const nx::vms::api::ModuleInformation& serverInfo)
    {
        const auto serverId = serverInfo.id;
        for (auto it = systems.begin(); it != systems.end(); ++it)
        {
            const auto cloudSystemId = it.key();
            const bool remove = (cloudSystemId != serverInfo.cloudSystemId);
            const auto system = it.value();
            if (remove && system->containsServer(serverId))
                system->removeServer(serverId);
        }
        return true;
    }

    void clearServersUnderLock(QnCloudSystemDescriptionPtr systemDescription)
    {
        const auto currentServers = systemDescription->servers();
        NX_ASSERT(currentServers.size() <= 1, "There should be one or zero servers");
        for (const auto& current: currentServers)
            systemDescription->removeServer(current.id);
    }

    bool parseModuleInformation(
        const QByteArray& messageBody,
        nx::vms::api::ModuleInformationWithAddresses* moduleInformation)
    {
        nx::network::rest::JsonResult jsonReply;
        if (!NX_ASSERT(QJson::deserialize(messageBody, &jsonReply)))
        {
            NX_DEBUG(this, "Failed to deserialize json reply:\n%1", messageBody);
            return false;
        }

        if (!NX_ASSERT(QJson::deserialize(jsonReply.reply, moduleInformation)))
        {
            NX_DEBUG(this, "Failed to deserialize module information:\n%1", messageBody);
            return false;
        }

        return true;
    }

    void onRequestComplete(const QString& cloudSystemId, ClientPool::ContextPtr context)
    {
        const bool success = context->hasSuccessfulResponse();
        if (success)
        {
             NX_VERBOSE(this, "Cloud system <%1>: ping successful", cloudSystemId);
        }
        else
        {
            NX_VERBOSE(this,
                "Cloud system <%1>: ping failure: status %2 error %3",
                cloudSystemId,
                context->getStatusCode(),
                context->systemError);
        }

        const NX_MUTEX_LOCKER lock(&mutex);
        const auto it = systems.find(cloudSystemId);
        if (it == systems.end())
            return;

        const auto systemDescription = it.value();
        nx::vms::api::ModuleInformationWithAddresses moduleInformation;

        if (!success
            || !parseModuleInformation(context->response.messageBody, &moduleInformation)
                // Prevent hanging of fake online cloud servers.
            || !tryRemoveAlienServerUnderLock(moduleInformation)
            || (cloudSystemId != moduleInformation.cloudSystemId)
            )
        {
            clearServersUnderLock(systemDescription);
            return;
        }

        const auto serverId = moduleInformation.id;
        if (systemDescription->containsServer(serverId))
        {
            systemDescription->updateServer(moduleInformation);
        }
        else
        {
            clearServersUnderLock(systemDescription);
            systemDescription->addServer(moduleInformation, /*priority*/ 0);
        }

        nx::utils::Url url;
        url.setHost(moduleInformation.cloudId());
        url.setScheme(nx::network::http::kSecureUrlSchemeName);
        systemDescription->setServerHost(serverId, url);
        NX_DEBUG(this,
            "Cloud system <%1>: set server <%2> url to %3",
            cloudSystemId,
            serverId.toString(),
            url.toString(QUrl::RemovePassword));
    }

    void updateStateUnderLock(const QnCloudSystemList& targetSystems)
    {
        for (const auto system: targetSystems)
        {
            const auto itCurrent = systems.find(system.cloudId);
            if (itCurrent == systems.end())
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

    void onCloudStatusChanged(QnCloudStatusWatcher::Status status)
    {
        setCloudSystems(status == QnCloudStatusWatcher::Online
            ? qnCloudStatusWatcher->cloudSystems()
            : QnCloudSystemList());
    }

    void setCloudSystems(const QnCloudSystemList& value)
    {
        Private::SystemsHash updatedSystems;
        for (const auto& system: value)
        {
            NX_ASSERT(!helpers::isNewSystem(system), "Cloud system can't be NEW system");

            const auto targetId = helpers::getTargetSystemId(system);
            const auto systemDescription = QnCloudSystemDescription::create(
                targetId,
                system.localId,
                system.name,
                system.ownerAccountEmail,
                system.ownerFullName,
                system.online,
                system.system2faEnabled);
            updatedSystems.insert(system.cloudId, systemDescription);
        }

        typedef QSet<QString> IdsSet;

        const auto newIds = nx::utils::toQSet(updatedSystems.keys());

        QHash<QString, QnUuid> removedTargetIds;

        {
            NX_MUTEX_LOCKER lock(&mutex);

            const auto oldIds = nx::utils::toQSet(systems.keys());
            const auto addedCloudIds = IdsSet(newIds).subtract(oldIds);

            const auto removedCloudIds = IdsSet(oldIds).subtract(newIds);
            for (const auto addedCloudId : addedCloudIds)
            {
                const auto system = updatedSystems[addedCloudId];
                emit q->systemDiscovered(system);

                systems.insert(addedCloudId, system);
                pingSystem(addedCloudId);
            }

            for (const auto removedCloudId : removedCloudIds)
            {
                const auto system = systems[removedCloudId];
                removedTargetIds.insert(system->id(), system->localId());
                systems.remove(removedCloudId);
            }

            updateStateUnderLock(value);
        }

        for (const auto id: removedTargetIds.keys())
        {
            const auto localId = removedTargetIds[id];
            emit q->systemLostInternal(id, localId);
            emit q->systemLost(id);
        }
    }
};

CloudSystemsFinder::CloudSystemsFinder(QObject* parent):
    base_type(parent),
    d(new Private{.q=this})
{
    NX_ASSERT(qnCloudStatusWatcher, "Cloud watcher is not ready");

    connect(qnCloudStatusWatcher,
        &QnCloudStatusWatcher::statusChanged,
        this,
        [this](auto status) { d->onCloudStatusChanged(status); });
    connect(qnCloudStatusWatcher,
        &QnCloudStatusWatcher::cloudSystemsChanged,
        this,
        [this](const auto& value) { d->setCloudSystems(value); });

    d->clientPool->setDefaultTimeouts(kDefaultTimeouts);

    auto updateSystemsTimer = new QTimer(this);
    updateSystemsTimer->callOnTimeout(
        [this] { d->pingAllSystems(); });
    updateSystemsTimer->setInterval(kCloudSystemsRefreshPeriod);
    updateSystemsTimer->start();

    d->onCloudStatusChanged(qnCloudStatusWatcher->status());
}

CloudSystemsFinder::~CloudSystemsFinder()
{
    d->clientPool->stop(/*invokeCallbacks*/ false);
}

QnAbstractSystemsFinder::SystemDescriptionList CloudSystemsFinder::systems() const
{
    SystemDescriptionList result;

    const NX_MUTEX_LOCKER lock(&d->mutex);
    for (const auto& system: d->systems)
        result.append(system.dynamicCast<QnBaseSystemDescription>());

    return result;
}

QnSystemDescriptionPtr CloudSystemsFinder::getSystem(const QString &id) const
{
    const NX_MUTEX_LOCKER lock(&d->mutex);

    const auto systemDescriptions = d->systems.values();
    const auto predicate =
        [id](const QnSystemDescriptionPtr &desc)
        {
            return (desc->id() == id);
        };

    const auto it = std::find_if(systemDescriptions.begin(), systemDescriptions.end(), predicate);

    return (it == systemDescriptions.end()
        ? QnSystemDescriptionPtr()
        : (*it).dynamicCast<QnBaseSystemDescription>());
}

} // namespace nx::vms::client::core
