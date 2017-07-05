#include "incompatible_server_watcher.h"

#include <common/common_module.h>

#include <client_core/connection_context_aware.h>

#include <client/client_message_processor.h>

#include <core/resource_management/resource_pool.h>
#include <core/resource/media_server_resource.h>

#include <network/connection_validator.h>

#include <nx/utils/log/log.h>
#include <common/common_module.h>
#include <core/resource/fake_media_server.h>

namespace {

bool isSuitable(const QnModuleInformation &moduleInformation)
{
    return moduleInformation.version >= QnSoftwareVersion(2, 3, 0, 0);
}

} // namespace

class QnIncompatibleServerWatcherPrivate : public QObject, public QnConnectionContextAware
{
    Q_DECLARE_PUBLIC(QnIncompatibleServerWatcher)
    QnIncompatibleServerWatcher *q_ptr;

public:
    QnIncompatibleServerWatcherPrivate(QnIncompatibleServerWatcher *parent);

    void at_resourcePool_statusChanged(const QnResourcePtr &resource);
    void at_discoveredServerChanged(const ec2::ApiDiscoveredServerData &serverData);

    void addResource(const ec2::ApiDiscoveredServerData &serverData);
    void removeResource(const QnUuid &id);
    QnUuid getFakeId(const QnUuid &realId) const;

    QnMediaServerResourcePtr makeResource(const ec2::ApiDiscoveredServerData &serverData) const;

public:
    struct DiscoveredServerItem {
        ec2::ApiDiscoveredServerData serverData;
        bool keep;
        bool removed;

        DiscoveredServerItem() :
            keep(false),
            removed(false)
        {}

        DiscoveredServerItem(const ec2::ApiDiscoveredServerData &serverData) :
            serverData(serverData),
            keep(false),
            removed(false)
        {}
    };

    mutable QnMutex mutex;
    QHash<QnUuid, QnUuid> fakeUuidByServerUuid;
    QHash<QnUuid, QnUuid> serverUuidByFakeUuid;
    QHash<QnUuid, DiscoveredServerItem> discoveredServerItemById;
};

QnIncompatibleServerWatcher::QnIncompatibleServerWatcher(QObject *parent) :
    QObject(parent),
    d_ptr(new QnIncompatibleServerWatcherPrivate(this))
{
}

QnIncompatibleServerWatcher::~QnIncompatibleServerWatcher()
{
    stop();
}

void QnIncompatibleServerWatcher::start()
{
    Q_D(QnIncompatibleServerWatcher);

    connect(qnClientMessageProcessor, &QnCommonMessageProcessor::discoveredServerChanged,
            d, &QnIncompatibleServerWatcherPrivate::at_discoveredServerChanged);

    connect(resourcePool(), &QnResourcePool::statusChanged,
            d, &QnIncompatibleServerWatcherPrivate::at_resourcePool_statusChanged);
}

void QnIncompatibleServerWatcher::stop()
{
    Q_D(QnIncompatibleServerWatcher);

    if (!commonModule())
        return;

    if (auto messageProcessor = commonModule()->messageProcessor())
        messageProcessor->disconnect(this);

    resourcePool()->disconnect(this);

    QList<QnUuid> ids;

    {
        QnMutexLocker lock(&d->mutex);
        ids = d->fakeUuidByServerUuid.values();
        d->fakeUuidByServerUuid.clear();
        d->serverUuidByFakeUuid.clear();
        d->discoveredServerItemById.clear();
    }

    for (const QnUuid& id: ids)
    {
        if (auto server = resourcePool()->getIncompatibleServerById(id))
            resourcePool()->removeResource(server);
    }
}

void QnIncompatibleServerWatcher::keepServer(const QnUuid &id, bool keep)
{
    Q_D(QnIncompatibleServerWatcher);

    QnMutexLocker lock(&d->mutex);

    auto it = d->discoveredServerItemById.find(id);
    if (it == d->discoveredServerItemById.end())
        return;

    it->keep = keep;

    if (!keep && it->removed)
    {
        d->discoveredServerItemById.erase(it);

        lock.unlock();

        d->removeResource(d->getFakeId(id));
    }
}

void QnIncompatibleServerWatcher::createInitialServers(
        const ec2::ApiDiscoveredServerDataList& discoveredServers)
{
    Q_D(QnIncompatibleServerWatcher);

    for (const ec2::ApiDiscoveredServerData &discoveredServer: discoveredServers)
        d->at_discoveredServerChanged(discoveredServer);
}


QnIncompatibleServerWatcherPrivate::QnIncompatibleServerWatcherPrivate(QnIncompatibleServerWatcher *parent) :
    QObject(parent),
    q_ptr(parent)
{
}

void QnIncompatibleServerWatcherPrivate::at_resourcePool_statusChanged(const QnResourcePtr &resource)
{
    QnMediaServerResourcePtr server = resource.dynamicCast<QnMediaServerResource>();
    if (!server)
        return;

    QnUuid id = server->getId();

    {
        QnMutexLocker lock(&mutex);
        if (serverUuidByFakeUuid.contains(id))
            return;
    }

    Qn::ResourceStatus status = server->getStatus();
    if (status != Qn::Offline
        && QnConnectionValidator::isCompatibleToCurrentSystem(server->getModuleInformation(),
            commonModule()))
    {
        removeResource(getFakeId(id));
    }
    else if (status == Qn::Offline)
    {
        QnMutexLocker lock(&mutex);
        auto it = discoveredServerItemById.find(id);
        if (it != discoveredServerItemById.end())
        {
            lock.unlock();

            if (it->serverData.status == Qn::Incompatible || it->serverData.status == Qn::Unauthorized)
                addResource(it->serverData);
        }
    }
}

void QnIncompatibleServerWatcherPrivate::at_discoveredServerChanged(
        const ec2::ApiDiscoveredServerData &serverData)
{
    QnMutexLocker lock(&mutex);
    auto it = discoveredServerItemById.find(serverData.id);

    switch (serverData.status)
    {
        case Qn::Online:
        case Qn::Incompatible:
        case Qn::Unauthorized:
        {
            if (it != discoveredServerItemById.end())
            {
                it->removed = false;
                it->serverData = serverData;
            }
            else
            {
                discoveredServerItemById.insert(serverData.id, serverData);
            }

            lock.unlock();

            const bool createResource =
                serverData.status == Qn::Incompatible
                    || (serverData.status == Qn::Unauthorized
                        && !resourcePool()->getResourceById<QnMediaServerResource>(serverData.id));

            if (createResource)
                addResource(serverData);
            else
                removeResource(getFakeId(serverData.id));

            break;
        }

        default:
        {
            if (it == discoveredServerItemById.end())
                break;

            it->removed = true;

            if (it->keep)
                break;

            discoveredServerItemById.remove(serverData.id);

            lock.unlock();

            removeResource(getFakeId(serverData.id));

            break;
        }
    }
}

void QnIncompatibleServerWatcherPrivate::addResource(const ec2::ApiDiscoveredServerData &serverData)
{
    QnUuid id = getFakeId(serverData.id);

    if (id.isNull())
    {
        // add a resource
        if (!isSuitable(serverData))
            return;

        NX_ASSERT(serverData.status != Qn::Offline, Q_FUNC_INFO, "Offline status is a mark to remove fake server.");
        QnMediaServerResourcePtr server = makeResource(serverData);
        {
            QnMutexLocker lock(&mutex);
            fakeUuidByServerUuid[serverData.id] = server->getId();
            serverUuidByFakeUuid[server->getId()] = serverData.id;
        }
        resourcePool()->addIncompatibleResource(server);

        NX_LOG(lit("QnIncompatibleServerWatcher: Add incompatible server %1 at %2 [%3]")
            .arg(serverData.id.toString())
            .arg(serverData.systemName)
            .arg(QStringList(serverData.remoteAddresses.toList()).join(lit(", "))),
            cl_logDEBUG1);
    }
    else
    {
        // update the resource
        auto server = resourcePool()->getIncompatibleServerById(id)
            .dynamicCast<QnFakeMediaServerResource>();

        NX_ASSERT(server, "There must be a resource in the resource pool.", Q_FUNC_INFO);

        if (!server)
            return;

        server->setFakeServerModuleInformation(serverData);

        NX_LOG(lit("QnIncompatibleServerWatcher: Update incompatible server %1 at %2 [%3]")
            .arg(serverData.id.toString())
            .arg(serverData.systemName)
            .arg(QStringList(serverData.remoteAddresses.toList()).join(lit(", "))),
            cl_logDEBUG1);
    }
}

void QnIncompatibleServerWatcherPrivate::removeResource(const QnUuid &id)
{
    if (id.isNull())
        return;

    QnUuid serverId;
    {
        QnMutexLocker lock(&mutex);
        serverId = serverUuidByFakeUuid.take(id);
        if (serverId.isNull())
            return;

        fakeUuidByServerUuid.remove(serverId);
    }


    if (auto server = resourcePool()->getIncompatibleServerById(id))
    {
        NX_LOG(lit("QnIncompatibleServerWatcher: Remove incompatible server %1 at %2")
            .arg(serverId.toString())
            .arg(server->getModuleInformation().systemName),
            cl_logDEBUG1);

        resourcePool()->removeResource(server);
    }
}

QnUuid QnIncompatibleServerWatcherPrivate::getFakeId(const QnUuid &realId) const
{
    QnMutexLocker lock(&mutex);
    return fakeUuidByServerUuid.value(realId);
}

QnMediaServerResourcePtr QnIncompatibleServerWatcherPrivate::makeResource(
    const ec2::ApiDiscoveredServerData& serverData) const
{
    QnFakeMediaServerResourcePtr server(new QnFakeMediaServerResource(commonModule()));
    server->setFakeServerModuleInformation(serverData);
    return server;
}
