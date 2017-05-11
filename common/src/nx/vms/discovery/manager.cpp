#include "manager.h"

#include <common/common_module.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/media_server_resource.h>
#include <nx/utils/log/log.h>

namespace nx {
namespace vms {
namespace discovery {

Manager::Manager(QnCommonModule* commonModule, bool clientMode, QnResourcePool* resourcePool):
    QnCommonModuleAware(commonModule),
    m_resourcePool(resourcePool)
{
    qRegisterMetaType<nx::vms::discovery::Manager::ModuleData>();
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

Manager::ModuleData::ModuleData()
{
}

Manager::ModuleData::ModuleData(QnModuleInformation old, SocketAddress endpoint):
    QnModuleInformation(std::move(old)),
    endpoint(std::move(endpoint))
{
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
            m_moduleConnector->diactivate();
            m_multicastFinder->stopWhileInAioThread();
            m_legacyMulticastFinder->pleaseStop();
        });
}

std::list<Manager::ModuleData> Manager::getAll() const
{
    QnMutexLocker lock(&m_mutex);
    std::list<Manager::ModuleData> list;
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

boost::optional<Manager::ModuleData> Manager::getModule(const QnUuid& id) const
{
    QnMutexLocker lock(&m_mutex);
    const auto it = m_modules.find(id);
    if (it == m_modules.end())
        return boost::none;

    return it->second;
}

void Manager::checkEndpoint(const SocketAddress& endpoint)
{
    m_moduleConnector->dispatch(
        [this, endpoint]() mutable
        {
            m_moduleConnector->newEndpoints({endpoint});
        });
}

void Manager::checkEndpoint(const QUrl& url)
{
    checkEndpoint(SocketAddress(url.host(), (uint16_t) url.port()));
}

static bool isChanged(const Manager::ModuleData& lhs, const Manager::ModuleData& rhs)
{
    return lhs.type == rhs.type
        && lhs.customization == rhs.customization
        && lhs.brand == rhs.brand
        && lhs.version == rhs.version
        && lhs.systemName == rhs.systemName
        && lhs.name == rhs.name
        && lhs.port == rhs.port
        && lhs.sslAllowed == rhs.sslAllowed
        && lhs.protoVersion == rhs.protoVersion
        && lhs.runtimeId == rhs.runtimeId
        && lhs.serverFlags == rhs.serverFlags
        && lhs.ecDbReadOnly == rhs.ecDbReadOnly
        && lhs.cloudSystemId == rhs.cloudSystemId
        && lhs.cloudPortalUrl == rhs.cloudPortalUrl
        && lhs.cloudHost == rhs.cloudHost
        && lhs.localSystemId == rhs.localSystemId
        && lhs.endpoint == rhs.endpoint;
}

void Manager::initializeConnector()
{
    m_moduleConnector = std::make_unique<ModuleConnector>();
    m_moduleConnector->setConnectHandler(
        [this](QnModuleInformation information, SocketAddress endpoint)
        {
            ModuleData module(std::move(information), std::move(endpoint));
            if (commonModule()->moduleGUID() == module.id)
                return conflict(module);

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
                    if (!isChanged(insert.first->second, module))
                        return;

                    insert.first->second = module;
                }
            }

            if (isNew)
            {
                NX_LOGX(lm("Found module %1, endpoint %2").strs(information.id, endpoint),
                    cl_logDEBUG1);

                emit found(module);
            }
            else
            {
                NX_LOGX(lm("Changed module %1, endpoint %2").strs(information.id, endpoint),
                    cl_logDEBUG1);

                emit changed(module);
            }
        });

    m_moduleConnector->setDisconnectHandler(
        [this](QnUuid id)
        {
            QnMutexLocker lock(&m_mutex);
            const auto it = m_modules.find(id);
            if (it != m_modules.end())
            {
                m_modules.erase(it);
                lock.unlock();

                NX_LOGX(lm("Lost module %1").strs(id), cl_logDEBUG1);
                emit lost(id);
            }
        });
}

void Manager::initializeMulticastFinders(bool clientMode)
{
    m_multicastFinder = std::make_unique<UdpMulticastFinder>(m_moduleConnector->getAioThread());
    m_multicastFinder->listen(
        [this](QnModuleInformationWithAddresses module)
        {
            if (module.id == commonModule()->moduleGUID() || module.remoteAddresses.empty())
            {
                NX_VERBOSE(this, lm("Reject multicast from %1 with %2 addresses").str(
                    module.id).container(module.remoteAddresses));
                return;
            }

            std::set<SocketAddress> endpoints;
            for (const auto& address: module.remoteAddresses)
                endpoints.insert(address);

            m_moduleConnector->newEndpoints(std::move(endpoints), module.id);
        });

    if (!clientMode)
    {
        connect(commonModule(), &QnCommonModule::moduleInformationChanged,
            [this]()
            {
                const auto id = commonModule()->moduleGUID();
                if (const auto s = m_resourcePool->getResourceById<QnMediaServerResource>(id))
                    m_multicastFinder->multicastInformation(s->getModuleInformationWithAddresses());
            });
    }

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
    connect(m_resourcePool, &QnResourcePool::resourceAdded, this,
        [this](const QnResourcePtr &resource)
        {
            if (const auto server = resource.dynamicCast<QnMediaServerResource>())
            {
                connect(server.data(), &QnMediaServerResource::auxUrlsChanged, this,
                    [this, server]()
                    {
                        if (server->getId() == commonModule()->moduleGUID())
                        {
                            return m_moduleConnector->post(
                                [this, module = server->getModuleInformationWithAddresses()]()
                                {
                                    m_multicastFinder->multicastInformation(module);
                                });
                        }

                        int port = server->getPort();
                        std::set<SocketAddress> endpoints;
                        for (const QUrl &url: server->getIgnoredUrls())
                            endpoints.emplace(url.host(), url.port(port));

                        m_moduleConnector->post(
                            [this, id = server->getId(), endpoints = std::move(endpoints)]()
                            {
                                m_moduleConnector->ignoreEndpoints(std::move(endpoints), id);
                            });
                    });
            }

        });

    connect(m_resourcePool, &QnResourcePool::resourceRemoved, this,
        [this](const QnResourcePtr &resource)
        {
            if (const auto server = resource.dynamicCast<QnMediaServerResource>())
                server->disconnect(this);
        });
}

} // namespace discovery
} // namespace vms
} // namespace nx
