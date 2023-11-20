// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "manager.h"

#include <core/resource_management/resource_pool.h>
#include <core/resource/media_server_resource.h>
#include <nx_ec/data/api_conversion_functions.h>
#include <nx/fusion/serialization/json.h>
#include <nx/network/address_resolver.h>
#include <nx/network/socket_global.h>
#include <nx/network/url/url_parse_helper.h>
#include <nx/utils/log/log.h>
#include <nx/vms/common/system_settings.h>
#include <nx/utils/scoped_connections.h>
#include <utils/common/synctime.h>

#include "deprecated_multicast_finder.h"
#include "module_connector.h"
#include "udp_multicast_finder.h"

namespace nx::vms::discovery {

ModuleEndpoint::ModuleEndpoint(
    nx::vms::api::ModuleInformationWithAddresses old, nx::network::SocketAddress endpoint)
    :
    nx::vms::api::ModuleInformationWithAddresses(std::move(old)),
    endpoint(std::move(endpoint))
{
    NX_ASSERT(!this->endpoint.address.toString().empty());
}

bool ModuleEndpoint::operator==(const ModuleEndpoint& rhs) const
{
    typedef const nx::vms::api::ModuleInformationWithAddresses& BaseRef;
    return BaseRef(*this) == BaseRef(rhs) && endpoint == rhs.endpoint;
}

struct Manager::Private
{
    Manager* const q;
    std::optional<ServerModeInfo> serverModeInfo;
    std::atomic<bool> isRunning;
    mutable nx::Mutex mutex;
    std::map<QnUuid, ModuleEndpoint> modules;
    std::unique_ptr<ModuleConnector> moduleConnector;
    std::unique_ptr<UdpMulticastFinder> multicastFinder;
    std::unique_ptr<DeprecatedMulticastFinder> legacyMulticastFinder;
    nx::utils::ScopedConnections resourcePoolConnections;

    Private(Manager* owner):
        q(owner)
    {
    }

    void initializeConnector()
    {
        moduleConnector = std::make_unique<ModuleConnector>();
        moduleConnector->setConnectHandler(
            [this](api::ModuleInformationWithAddresses information,
                nx::network::SocketAddress requestedEndpoint,
                nx::network::SocketAddress resolvedEndpoint)
            {
                NX_VERBOSE(this, "Received module info: %1", QJson::serialized(information));

                NX_ASSERT(!requestedEndpoint.address.toString().empty());
                ModuleEndpoint module(std::move(information), std::move(requestedEndpoint));

                if (serverModeInfo && serverModeInfo->peerId == module.id)
                {
                    const auto runtimeId = serverModeInfo->sessionId;
                    if (module.runtimeId == runtimeId)
                        return; // Ignore own record.

                    NX_DEBUG(this, "Conflicting module %1 found on %2",
                        module.id,
                        module.endpoint);
                    emit q->conflict(qnSyncTime->currentTimePoint(), module);
                    return;
                }

                const nx::String newCloudHost = module.cloudId();
                nx::String oldCloudHost;
                bool isNew = true;
                {
                    NX_MUTEX_LOCKER lock(&mutex);
                    const auto insert = modules.emplace(module.id, module);
                    if (!insert.second)
                    {
                        if (insert.first->second == module)
                            return;

                        oldCloudHost = insert.first->second.cloudId();
                        insert.first->second = module;

                        isNew = false;
                    }
                }

                if (isNew)
                {
                    NX_DEBUG(this, "Found module %1 on endpoint %2", module.id, module.endpoint);
                    emit q->found(module);
                }
                else
                {
                    NX_DEBUG(this, "Changed module %1 on endpoint %2", module.id, module.endpoint);
                    emit q->changed(module);
                }

                // Delete fixed addresses only if module cloud address changed.
                auto& resolver = nx::network::SocketGlobals::addressResolver();
                if (!oldCloudHost.isEmpty() && oldCloudHost != newCloudHost)
                    resolver.removeFixedAddress(oldCloudHost);

                // Add fixed address only if not a cloud address used for connection to avoid loop.
                // It is impossible to fix a resolved address for DNS, since it is not always
                // possible to establish a connection at such an address.
                if (!newCloudHost.isEmpty()
                    && !resolver.isCloudHostname(module.endpoint.address.toString())
                    && module.endpoint.address.isIpAddress()
                    && resolvedEndpoint.address.isIpAddress()
                    && resolvedEndpoint.port > 0)
                {
                    resolver.addFixedAddress(newCloudHost, resolvedEndpoint);
                }
            });

        moduleConnector->setDisconnectHandler(
            [this](QnUuid id)
            {
                NX_MUTEX_LOCKER lock(&mutex);
                const auto it = modules.find(id);
                if (it != modules.end())
                {
                    const nx::String cloudHost = it->second.cloudId();
                    modules.erase(it);
                    lock.unlock();

                    NX_DEBUG(this, "Lost module %1", id);
                    emit q->lost(id);

                    if (!cloudHost.isEmpty())
                        nx::network::SocketGlobals::addressResolver().removeFixedAddress(cloudHost);
                }
            });
    }

    void initializeMulticastFinders()
    {
        multicastFinder = std::make_unique<UdpMulticastFinder>(moduleConnector->getAioThread());
        multicastFinder->listen(
            [this](nx::vms::api::ModuleInformationWithAddresses module,
                nx::network::SocketAddress /*endpoint*/)
            {
                const auto endpoints = ec2::moduleInformationEndpoints(module);
                const bool isOwnServer = serverModeInfo && (module.id == serverModeInfo->peerId);

                if (!isOwnServer && endpoints.size() != 0)
                {
                    moduleConnector->newEndpoints(
                        {endpoints.cbegin(), endpoints.cend()},
                        module.id);
                }
            });

        DeprecatedMulticastFinder::Options deprecatedMulticastOptions;

        if (serverModeInfo)
        {
            auto isMulticastEnabledFunction =
                [this] { return serverModeInfo->multicastDiscoveryAllowed; };

            multicastFinder->setIsMulticastEnabledFunction(isMulticastEnabledFunction);
            deprecatedMulticastOptions.responseEnabled = isMulticastEnabledFunction;
            deprecatedMulticastOptions.listenAndRespond = true;
            deprecatedMulticastOptions.peerId = serverModeInfo->peerId;
        }
        else //< Client mode.
        {
            multicastFinder->setIsMulticastEnabledFunction([]() { return false; });
            deprecatedMulticastOptions.multicastCount = 5;
            deprecatedMulticastOptions.listenAndRespond = false;
        }

        legacyMulticastFinder = std::make_unique<DeprecatedMulticastFinder>(
            deprecatedMulticastOptions);
        connect(legacyMulticastFinder.get(), &DeprecatedMulticastFinder::responseReceived,
            [this](const nx::vms::api::ModuleInformation& module,
                const nx::network::SocketAddress &endpoint, const nx::network::HostAddress& ip)
            {
                moduleConnector->newEndpoints(
                    {nx::network::SocketAddress(ip, endpoint.port)},
                    module.id);
            });
    }

    void initialize()
    {
        qRegisterMetaType<nx::vms::discovery::ModuleEndpoint>();
        initializeConnector();
        initializeMulticastFinders();
    }
};

Manager::Manager(QObject* parent):
    base_type(parent),
    d(new Private(this))
{
    d->initialize();
}

Manager::Manager(ServerModeInfo serverModeInfo, QObject* parent):
    base_type(parent),
    d(new Private(this))
{
    d->serverModeInfo = serverModeInfo;
    d->initialize();
}

Manager::~Manager()
{
    stop();
    d->legacyMulticastFinder->stop();
    d->multicastFinder->pleaseStopSync();
    d->moduleConnector->pleaseStopSync();
}

void Manager::setMulticastDiscoveryAllowed(bool value)
{
    if (NX_ASSERT(d->serverModeInfo))
        d->serverModeInfo->multicastDiscoveryAllowed = value;
}

void Manager::setMulticastInformation(const api::ModuleInformationWithAddresses& information)
{
    d->multicastFinder->multicastInformation(information);
}

void Manager::setReconnectPolicy(nx::network::RetryPolicy value)
{
    d->moduleConnector->setReconnectPolicy(value);
}

void Manager::setUpdateInterfacesInterval(std::chrono::milliseconds value)
{
    d->multicastFinder->setUpdateInterfacesInterval(value);
}

void Manager::setMulticastInterval(std::chrono::milliseconds value)
{
    d->multicastFinder->setSendInterval(value);
}

void Manager::start(QnResourcePool* resourcePool)
{
    d->moduleConnector->post(
        [this]()
        {
            d->moduleConnector->activate();
            d->multicastFinder->updateInterfaces();
            d->legacyMulticastFinder->start();
        });

    auto handleServerAdded =
        [this](const QnMediaServerResourcePtr& server)
        {
            updateEndpoints(server);
            d->resourcePoolConnections << connect(
                server.data(),
                &QnMediaServerResource::auxUrlsChanged,
                this,
                [this, server]() { updateEndpoints(server); });
        };

    d->resourcePoolConnections << connect(
        resourcePool,
        &QnResourcePool::resourceAdded,
        this,
        [handleServerAdded](const QnResourcePtr& resource)
        {
            if (const auto server = resource.dynamicCast<QnMediaServerResource>())
                handleServerAdded(server);
        });

    d->resourcePoolConnections << connect(
        resourcePool,
        &QnResourcePool::resourceRemoved,
        this,
        [this](const QnResourcePtr& resource)
        {
            if (const auto server = resource.dynamicCast<QnMediaServerResource>())
                server->disconnect(this);
        });

    for (const auto& server: resourcePool->servers())
        handleServerAdded(server);
}

void Manager::stop()
{
    d->resourcePoolConnections.reset();
    d->moduleConnector->post(
        [this]()
        {
            d->moduleConnector->deactivate();
            d->multicastFinder->stopWhileInAioThread();
            d->legacyMulticastFinder->pleaseStop();
        });
}

std::list<ModuleEndpoint> Manager::getAll() const
{
    NX_MUTEX_LOCKER lock(&d->mutex);
    std::list<ModuleEndpoint> list;
    for (const auto& m: d->modules)
        list.push_back(m.second);

    return list;
}

std::optional<nx::network::SocketAddress> Manager::getEndpoint(const QnUuid& id) const
{
    NX_MUTEX_LOCKER lock(&d->mutex);
    const auto it = d->modules.find(id);
    if (it == d->modules.end())
        return std::nullopt;

    return it->second.endpoint;
}

std::optional<ModuleEndpoint> Manager::getModule(const QnUuid& id) const
{
    NX_MUTEX_LOCKER lock(&d->mutex);
    const auto it = d->modules.find(id);
    if (it == d->modules.end())
        return std::nullopt;

    return it->second;
}

void Manager::forgetModule(const QnUuid& id)
{
    d->moduleConnector->forgetModule(id);
}

void Manager::checkEndpoint(nx::network::SocketAddress endpoint, QnUuid expectedId)
{
    NX_ASSERT(ModuleConnector::isValidForConnect(endpoint), "Invalid endpoint: %1", endpoint);
    d->moduleConnector->dispatch(
        [this, endpoint = std::move(endpoint), expectedId = std::move(expectedId)]() mutable
        {
            d->moduleConnector->newEndpoints({std::move(endpoint)}, std::move(expectedId));
        });
}

void Manager::checkEndpoint(const nx::utils::Url& url, QnUuid expectedId)
{
    checkEndpoint(
        nx::network::url::getEndpoint(url),
        std::move(expectedId));
}

void Manager::updateEndpoints(const QnMediaServerResourcePtr& server)
{
    const bool isOwnServer = d->serverModeInfo && (server->getId() == d->serverModeInfo->peerId);

    if (isOwnServer)
    {
        d->moduleConnector->post(
            [this, module = server->getModuleInformationWithAddresses()]()
            {
                d->multicastFinder->multicastInformation(module);
            });

        return;
    }

    auto port = (uint16_t) server->getPort();
    if (port == 0)
        return;

    std::set<nx::network::SocketAddress> allowedEndpoints;
    for (const auto& endpoint: server->getNetAddrList())
        allowedEndpoints.emplace(endpoint.address, endpoint.port ? endpoint.port : port);

    for (const auto& url: server->getAdditionalUrls())
        allowedEndpoints.emplace(nx::network::url::getEndpoint(url, port));

    if (auto address = server->getCloudAddress())
        allowedEndpoints.emplace(std::move(*address));

    std::set<nx::network::SocketAddress> forbiddenEndpoints;
    for (const auto& url: server->getIgnoredUrls())
        forbiddenEndpoints.emplace(nx::network::url::getEndpoint(url, port));

    d->moduleConnector->post(
        [this, id = server->getId(), allowed = std::move(allowedEndpoints),
            forbidden = std::move(forbiddenEndpoints)]() mutable
        {
            NX_DEBUG(this, "Server %1 resource endpoints: add %2, forbid %3",
                id,
                containerString(allowed),
                containerString(forbidden));

            d->moduleConnector->setForbiddenEndpoints(std::move(forbidden), id);
            if (allowed.size() != 0)
                d->moduleConnector->newEndpoints(allowed, id);
        });
}

} // namespace nx::vms::discovery
