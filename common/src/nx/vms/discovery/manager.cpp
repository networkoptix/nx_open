#include "manager.h"

#include <api/global_settings.h>
#include <common/common_module.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/media_server_resource.h>

#include <nx/network/cloud/address_resolver.h>
#include <nx/network/socket_global.h>
#include <nx/utils/log/log.h>

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

void Manager::setReconnectPolicy(network::RetryPolicy value)
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

ModuleEndpoint::ModuleEndpoint(QnModuleInformation old, SocketAddress endpoint):
    QnModuleInformation(std::move(old)),
    endpoint(std::move(endpoint))
{
}

bool ModuleEndpoint::operator==(const ModuleEndpoint& rhs) const
{
    typedef const QnModuleInformation& BaseRef;
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

boost::optional<SocketAddress> Manager::getEndpoint(const QnUuid& id) const
{
    QnMutexLocker lock(&m_mutex);
    const auto it = m_modules.find(id);
    if (it == m_modules.end())
        return boost::none;

    return it->second.endpoint;
}

boost::optional<ModuleEndpoint> Manager::getModule(const QnUuid& id) const
{
    QnMutexLocker lock(&m_mutex);
    const auto it = m_modules.find(id);
    if (it == m_modules.end())
        return boost::none;

    return it->second;
}

void Manager::checkEndpoint(SocketAddress endpoint, QnUuid expectedId)
{
    NX_EXPECT(nx::network::SocketGlobals::addressResolver().isValidForConnect(endpoint),
        lm("Invalid endpoint: %1").arg(endpoint));

    m_moduleConnector->dispatch(
        [this, endpoint = std::move(endpoint), expectedId = std::move(expectedId)]() mutable
        {
            m_moduleConnector->newEndpoints({std::move(endpoint)}, std::move(expectedId));
        });
}

void Manager::checkEndpoint(const QUrl& url, QnUuid expectedId)
{
    checkEndpoint(SocketAddress(url.host(), (uint16_t) url.port()), std::move(expectedId));
}

void Manager::initializeConnector()
{
    m_moduleConnector = std::make_unique<ModuleConnector>();
    m_moduleConnector->setConnectHandler(
        [this](QnModuleInformation information, SocketAddress endpoint, HostAddress ip)
        {
            if (!commonModule())
                return;

            ModuleEndpoint module(std::move(information), std::move(endpoint));
            if (commonModule()->moduleGUID() == module.id)
            {
                const auto runtimeId = commonModule()->runningInstanceGUID();
                if (module.runtimeId == runtimeId)
                    return; // Ignore own record.

                NX_DEBUG(this, lm("Conflict module %1 found on %2").args(module.id, module.endpoint));
                return conflict(module);
            }

            const QString newCloudHost = information.cloudId();
            QString oldCloudHost;
            bool isNew = false;
            {
                QnMutexLocker lock(&m_mutex);
                const auto insert = m_modules.emplace(module.id, module);
                if (insert.second)
                {
                    isNew = true;
                }
                else
                {
                    if (insert.first->second == module)
                        return;

                    oldCloudHost = insert.first->second.cloudId();
                    insert.first->second = module;
                }
            }

            if (isNew)
            {
                NX_DEBUG(this, lm("Found module %1 endpoint %2").args(information.id, endpoint));
                emit found(module);
            }
            else
            {
                NX_DEBUG(this, lm("Changed module %1 endpoint %2").args(information.id, endpoint));
                emit changed(module);
            }

            auto& resolver = network::SocketGlobals::addressResolver();
            if (!oldCloudHost.isEmpty())
                resolver.removeFixedAddress(oldCloudHost);

            if (!newCloudHost.isEmpty() && ip.isIpAddress())
                resolver.addFixedAddress(newCloudHost, SocketAddress(ip.toString(), endpoint.port));
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

                NX_LOGX(lm("Lost module %1").args(id), cl_logDEBUG1);
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
        [this](QnModuleInformationWithAddresses module, SocketAddress /*endpoint*/)
        {
            m_moduleConnector->newEndpoints(module.endpoints(), module.id);
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

    m_legacyMulticastFinder = new DeprecatedMulticastFinder(this, options);
    connect(m_legacyMulticastFinder, &DeprecatedMulticastFinder::responseReceived,
        [this](const QnModuleInformation &module,
            const SocketAddress &endpoint, const HostAddress& ip)
        {
            m_moduleConnector->newEndpoints({SocketAddress(ip, endpoint.port)}, module.id);
        });
}

void Manager::monitorServerUrls()
{
    connect(resourcePool(), &QnResourcePool::resourceAdded, this,
        [this](const QnResourcePtr &resource)
        {
            if (const auto server = resource.dynamicCast<QnMediaServerResource>())
            {
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

    std::set<SocketAddress> allowedEndpoints;
    for (const auto& endpoint: server->getNetAddrList())
        allowedEndpoints.emplace(endpoint.address, endpoint.port ? endpoint.port : port);

    for (const auto& url: server->getAdditionalUrls())
        allowedEndpoints.emplace(url.host(), (uint16_t) url.port(port));

    if (auto address = server->getCloudAddress())
        allowedEndpoints.emplace(std::move(*address));

    std::set<SocketAddress> forbiddenEndpoints;
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
