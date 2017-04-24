#include "manager.h"

#include "common/common_module.h"

namespace nx {
namespace vms {
namespace discovery {

Manager::Manager(QnCommonModule* commonModule, bool clientMode):
    QnCommonModuleAware(commonModule)
{
    initializeConnector();
    initializeMulticastFinders(clientMode);
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

void Manager::checkUrl(const SocketAddress& endpoint)
{
    m_moduleConnector->newEndpoint(endpoint);
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

} // namespace discovery
} // namespace vms
} // namespace nx
