#include "manager.h"

#include <api/global_settings.h>
#include <common/common_module.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/media_server_resource.h>
#include <nx_ec/data/api_conversion_functions.h>

#include <nx/network/address_resolver.h>
#include <nx/network/socket_global.h>
#include <nx/utils/log/log.h>
#include <nx/fusion/serialization/json.h>

namespace nx {
namespace vms {
namespace discovery {

Manager::Manager(bool clientMode, QObject* parent):
    base_type(parent),
    QnCommonModuleAware(parent)
{
    qRegisterMetaType<nx::vms::discovery::ModuleEndpoint>();
    initializeConnector();
    initializeMulticastFinders(clientMode);
    monitorServerUrls();
}

Manager::~Manager()
{
    m_legacyMulticastFinder->stop();
    m_multicastFinder->pleaseStopSync();
    m_moduleConnector->pleaseStopSync();
}

void Manager::setReconnectPolicy(nx::network::RetryPolicy value)
{
    m_moduleConnector->setReconnectPolicy(value);
}

void Manager::setUpdateInterfacesInterval(std::chrono::milliseconds value)
{
    m_multicastFinder->setUpdateInterfacesInterval(value);
}

void Manager::setMulticastInterval(std::chrono::milliseconds value)
{
    m_multicastFinder->setSendInterval(value);
}

ModuleEndpoint::ModuleEndpoint(
    nx::vms::api::ModuleInformation old, nx::network::SocketAddress endpoint)
    :
    nx::vms::api::ModuleInformation(std::move(old)),
    endpoint(std::move(endpoint))
{
    NX_ASSERT(!this->endpoint.address.toString().isEmpty());
}

bool ModuleEndpoint::operator==(const ModuleEndpoint& rhs) const
{
    typedef const nx::vms::api::ModuleInformation& BaseRef;
    return BaseRef(*this) == BaseRef(rhs) && endpoint == rhs.endpoint;
}

void Manager::start()
{
    m_moduleConnector->post(
        [this]()
        {
            m_moduleConnector->activate();
            m_multicastFinder->updateInterfaces();
            m_legacyMulticastFinder->start();
        });
}

void Manager::stop()
{
    m_moduleConnector->post(
        [this]()
        {
            m_moduleConnector->deactivate();
            m_multicastFinder->stopWhileInAioThread();
            m_legacyMulticastFinder->pleaseStop();
        });
}

std::list<ModuleEndpoint> Manager::getAll() const
{
    QnMutexLocker lock(&m_mutex);
    std::list<ModuleEndpoint> list;
    for (const auto& m: m_modules)
        list.push_back(m.second);

    return list;
}

std::optional<nx::network::SocketAddress> Manager::getEndpoint(const QnUuid& id) const
{
    QnMutexLocker lock(&m_mutex);
    const auto it = m_modules.find(id);
    if (it == m_modules.end())
        return std::nullopt;

    return it->second.endpoint;
}

std::optional<ModuleEndpoint> Manager::getModule(const QnUuid& id) const
{
    QnMutexLocker lock(&m_mutex);
    const auto it = m_modules.find(id);
    if (it == m_modules.end())
        return std::nullopt;

    return it->second;
}

void Manager::forgetModule(const QnUuid& id)
{
    m_moduleConnector->forgetModule(id);
}

void Manager::checkEndpoint(nx::network::SocketAddress endpoint, QnUuid expectedId)
{
    NX_ASSERT(nx::network::SocketGlobals::addressResolver().isValidForConnect(endpoint),
        lm("Invalid endpoint: %1").arg(endpoint));

    m_moduleConnector->dispatch(
        [this, endpoint = std::move(endpoint), expectedId = std::move(expectedId)]() mutable
        {
            m_moduleConnector->newEndpoints({std::move(endpoint)}, std::move(expectedId));
        });
}

void Manager::checkEndpoint(const nx::utils::Url& url, QnUuid expectedId)
{
    checkEndpoint(nx::network::SocketAddress(url.host(), (uint16_t) url.port()), std::move(expectedId));
}

void Manager::initializeConnector()
{
    m_moduleConnector = std::make_unique<ModuleConnector>();
    m_moduleConnector->setConnectHandler(
        [this](api::ModuleInformation information, network::SocketAddress requestedEndpoint,
            network::SocketAddress resolvedEndpoint)
        {
            NX_VERBOSE(this, lm("Received module info: %1").arg(QJson::serialized(information)));

            if (!commonModule())
                return;

            NX_ASSERT(!requestedEndpoint.address.toString().isEmpty());
            ModuleEndpoint module(std::move(information), std::move(requestedEndpoint));
            if (commonModule()->moduleGUID() == module.id)
            {
                const auto runtimeId = commonModule()->runningInstanceGUID();
                if (module.runtimeId == runtimeId)
                    return; // Ignore own record.

                NX_DEBUG(this, lm("Conflict module %1 found on %2").args(module.id, module.endpoint));
                return conflict(module);
            }

            const QString newCloudHost = module.cloudId();
            QString oldCloudHost;
            bool isNew = true;
            {
                QnMutexLocker lock(&m_mutex);
                const auto insert = m_modules.emplace(module.id, module);
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
                emit found(module);
            }
            else
            {
                NX_DEBUG(this, "Changed module %1 on endpoint %2", module.id, module.endpoint);
                emit changed(module);
            }

            // Delete fixed addresses only if module cloud address changed.
            auto& resolver = network::SocketGlobals::addressResolver();
            if (!oldCloudHost.isEmpty() && oldCloudHost != newCloudHost)
                resolver.removeFixedAddress(oldCloudHost);

            // Add fixed address only if not a cloud address used for connection to avoid loop.
            if (!newCloudHost.isEmpty()
                && !resolver.isCloudHostName(module.endpoint.address.toString())
                && NX_ASSERT(resolvedEndpoint.address.isIpAddress())
                && NX_ASSERT(resolvedEndpoint.port > 0))
            {
                resolver.addFixedAddress(newCloudHost, resolvedEndpoint);
            }
        });

    m_moduleConnector->setDisconnectHandler(
        [this](QnUuid id)
        {
            QnMutexLocker lock(&m_mutex);
            const auto it = m_modules.find(id);
            if (it != m_modules.end())
            {
                const auto cloudHost = it->second.cloudId();
                m_modules.erase(it);
                lock.unlock();

                NX_DEBUG(this, lm("Lost module %1").args(id));
                emit lost(id);

                if (!cloudHost.isEmpty())
                    network::SocketGlobals::addressResolver().removeFixedAddress(cloudHost);
            }
        });
}

void Manager::initializeMulticastFinders(bool clientMode)
{
    m_multicastFinder = std::make_unique<UdpMulticastFinder>(m_moduleConnector->getAioThread());
    m_multicastFinder->listen(
        [this](nx::vms::api::ModuleInformationWithAddresses module,
            nx::network::SocketAddress /*endpoint*/)
        {
            const auto endpoints = ec2::moduleInformationEndpoints(module);
            m_moduleConnector->newEndpoints({endpoints.cbegin(), endpoints.cend()}, module.id);
        });

    if (!clientMode)
    {
        connect(commonModule(), &QnCommonModule::moduleInformationChanged,
            [this]()
            {
                const auto id = commonModule()->moduleGUID();
                if (const auto s = resourcePool()->getResourceById<QnMediaServerResource>(id))
                    m_multicastFinder->multicastInformation(s->getModuleInformationWithAddresses());
            });
    }

    m_multicastFinder->setIsMulticastEnabledFunction(
        [common = commonModule()]()
        {
            if (const auto settings = common->globalSettings())
                return !settings->isInitialized() || settings->isAutoDiscoveryEnabled();

            return false;
        });

    DeprecatedMulticastFinder::Options options;
    if (clientMode)
        options.multicastCount = 5;
    else
        options.listenAndRespond = true;

    options.responseEnabled =
        [common = commonModule()]()
        {
            if (const auto settings = common->globalSettings())
                return !settings->isInitialized() || settings->isAutoDiscoveryResponseEnabled();

            return false;
        };

    m_legacyMulticastFinder = new DeprecatedMulticastFinder(this, options);
    connect(m_legacyMulticastFinder, &DeprecatedMulticastFinder::responseReceived,
        [this](const nx::vms::api::ModuleInformation& module,
            const nx::network::SocketAddress &endpoint, const nx::network::HostAddress& ip)
        {
            m_moduleConnector->newEndpoints(
                {nx::network::SocketAddress(ip, endpoint.port)}, module.id);
        });
}

void Manager::monitorServerUrls()
{
    connect(resourcePool(), &QnResourcePool::resourceAdded, this,
        [this](const QnResourcePtr &resource)
        {
            if (const auto server = resource.dynamicCast<QnMediaServerResource>())
            {
                // Skip endpoints from servers, discovered by the remote server.
                if (server->hasFlags(Qn::fake_server))
                    return;

                updateEndpoints(server.data());
                connect(server.data(), &QnMediaServerResource::auxUrlsChanged,
                    this, [this, server]() { updateEndpoints(server.data()); });
            }
        });

    connect(resourcePool(), &QnResourcePool::resourceRemoved, this,
        [this](const QnResourcePtr &resource)
        {
            if (const auto server = resource.dynamicCast<QnMediaServerResource>())
                server->disconnect(this);
        });
}

void Manager::updateEndpoints(const QnMediaServerResource* server)
{
    if (server->getId() == commonModule()->moduleGUID())
    {
        return m_moduleConnector->post(
            [this, module = server->getModuleInformationWithAddresses()]()
            {
                m_multicastFinder->multicastInformation(module);
            });
    }

    auto port = (uint16_t) server->getPort();
    if (port == 0)
        return;

    std::set<nx::network::SocketAddress> allowedEndpoints;
    for (const auto& endpoint: server->getNetAddrList())
        allowedEndpoints.emplace(endpoint.address, endpoint.port ? endpoint.port : port);

    for (const auto& url: server->getAdditionalUrls())
        allowedEndpoints.emplace(url.host(), (uint16_t) url.port(port));

    if (auto address = server->getCloudAddress())
        allowedEndpoints.emplace(std::move(*address));

    std::set<nx::network::SocketAddress> forbiddenEndpoints;
    for (const auto& url: server->getIgnoredUrls())
        forbiddenEndpoints.emplace(url.host(), (uint16_t) url.port(port));

    m_moduleConnector->post(
        [this, id = server->getId(), allowed = std::move(allowedEndpoints),
            forbidden = std::move(forbiddenEndpoints)]() mutable
        {
            NX_DEBUG(this, lm("Server %1 resource endpoints: add %2, forbid %3")
                .arg(id).container(allowed).container(forbidden));

            m_moduleConnector->setForbiddenEndpoints(std::move(forbidden), id);
            if (allowed.size() != 0)
                m_moduleConnector->newEndpoints(allowed, id);
        });
}

} // namespace discovery
} // namespace vms
} // namespace nx
