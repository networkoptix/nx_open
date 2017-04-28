#include "manager.h"

#include "common/common_module.h"
#include "core/resource_management/resource_pool.h"
#include "core/resource/media_server_resource.h"

namespace nx {
namespace vms {
namespace discovery {

Manager::Manager(QnCommonModule* commonModule, bool clientMode, QnResourcePool* resourcePool):
    QnCommonModuleAware(commonModule)
{
    initializeConnector();
    initializeMulticastFinders(clientMode);
    monitorServerUrls(resourcePool);
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
    m_moduleConnector->post([this, endpoint](){ m_moduleConnector->newEndpoint(endpoint); });
}

void Manager::checkEndpoint(const QUrl& url)
{
    checkEndpoint(SocketAddress(url.host(), (uint16_t) url.port()));
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
            ModuleData* iterator = nullptr;
            {
                QnMutexLocker lock(&m_mutex);
                const auto insert = m_modules.emplace(module.id, module);
                iterator = &insert.first->second;
                isNew = insert.second;
            }

            if (isNew)
                return found(module);

            if (*iterator != module)
                return changed(module);

            NX_ASSERT(false);
        });

    m_moduleConnector->setDisconnectHandler(
        [this](QnUuid id)
        {
            {
                QnMutexLocker lock(&m_mutex);
                m_modules.erase(id);
            }

            emit lost(id);
        });
}

void Manager::initializeMulticastFinders(bool clientMode)
{
    m_multicastFinder = std::make_unique<UdpMulticastFinder>(m_moduleConnector->getAioThread());
    m_multicastFinder->listen(
        [this](QnModuleInformationWithAddresses module)
        {
            for (const auto& address: module.remoteAddresses)
                m_moduleConnector->newEndpoint(address, module.id);
        });

    if (!clientMode)
    {
        const auto updateMf =
            [this](){ m_multicastFinder->multicastInformation(commonModule()->moduleInformation()); };

        connect(commonModule(), &QnCommonModule::moduleInformationChanged, updateMf);
        updateMf();
    }

    QnMulticastModuleFinder::Options options;
    if (clientMode)
        options.multicastCount = 5;
    else
        options.listenAndRespond = true;

    m_legacyMulticastFinder = new QnMulticastModuleFinder(this, options);
    connect(m_legacyMulticastFinder, &QnMulticastModuleFinder::responseReceived,
        [this](const QnModuleInformation &module,
            const SocketAddress &endpoint, const HostAddress& ip)
        {
            m_moduleConnector->newEndpoint(SocketAddress(ip, endpoint.port), module.id);
        });
}

void Manager::monitorServerUrls(QnResourcePool* resourcePool)
{
    connect(resourcePool, &QnResourcePool::resourceAdded, this,
        [this](const QnResourcePtr &resource)
        {
            if (const auto server = resource.dynamicCast<QnMediaServerResource>())
            {
                connect(server.data(), &QnMediaServerResource::auxUrlsChanged, this,
                    [this, server]()
                    {
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

    connect(commonModule()->resourcePool(), &QnResourcePool::resourceRemoved, this,
        [this](const QnResourcePtr &resource)
        {
            if (const auto server = resource.dynamicCast<QnMediaServerResource>())
                server->disconnect(this);
        });
}

} // namespace discovery
} // namespace vms
} // namespace nx
