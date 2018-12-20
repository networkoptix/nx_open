#include "discovery_monitor.h"
#include <transaction/message_bus_adapter.h>
#include <nx_ec/data/api_conversion_functions.h>
#include <network/connection_validator.h>
#include <common/common_module.h>
#include <api/global_settings.h>

namespace nx {
namespace vms::server {
namespace discovery {

using namespace nx::vms;

static api::DiscoveredServerData makeServer(
    const nx::vms::discovery::ModuleEndpoint& module, const QnUuid& localSystemId)
{
    api::DiscoveredServerData serverData(module);
    ec2::setModuleInformationEndpoints(serverData, {module.endpoint});

    if (QnConnectionValidator::validateConnection(module) != Qn::SuccessConnectionResult)
    {
        serverData.status = api::ResourceStatus::incompatible;
    }
    else
    {
        serverData.status = (!localSystemId.isNull() && module.localSystemId == localSystemId)
            ? api::ResourceStatus::online
            : api::ResourceStatus::unauthorized;
    }

    return serverData;
}

api::DiscoveredServerDataList getServers(nx::vms::discovery::Manager* manager)
{
    const auto localSystemId = manager->commonModule()->globalSettings()->localSystemId();

    api::DiscoveredServerDataList result;
    for (const auto& module: manager->getAll())
        result.push_back(makeServer(module, localSystemId));

    return result;
}

DiscoveryMonitor::DiscoveryMonitor(ec2::TransactionMessageBusAdapter* messageBus):
    QnCommonModuleAware(messageBus->commonModule()),
    m_messageBus(messageBus)
{
    QObject::connect(messageBus, &ec2::QnTransactionMessageBus::peerFound,
        this, &DiscoveryMonitor::clientFound);

    commonModule()->moduleDiscoveryManager()->onSignals(this, &DiscoveryMonitor::serverFound,
        &DiscoveryMonitor::serverFound, &DiscoveryMonitor::serverLost);
}

DiscoveryMonitor::~DiscoveryMonitor()
{
    m_messageBus->disconnect(this);
    commonModule()->moduleDiscoveryManager()->disconnect(this);
}

void DiscoveryMonitor::clientFound(QnUuid peerId, api::PeerType peerType)
{
    if (!api::PeerData::isClient(peerType))
        return;

    send(ec2::ApiCommand::discoveredServersList,
        getServers(commonModule()->moduleDiscoveryManager()), peerId);
}

void DiscoveryMonitor::serverFound(nx::vms::discovery::ModuleEndpoint module)
{
    const auto server = makeServer(module, globalSettings()->localSystemId());
    m_serverCache.emplace(server.id, server);
    send(ec2::ApiCommand::discoveredServerChanged, server, m_messageBus->directlyConnectedClientPeers());
}

void DiscoveryMonitor::serverLost(QnUuid id)
{
    const auto it = m_serverCache.find(id);
    if (it == m_serverCache.end())
        return;

    auto server = it->second;
    m_serverCache.erase(it);

    server.status = api::ResourceStatus::offline;
    send(ec2::ApiCommand::discoveredServerChanged, server, m_messageBus->directlyConnectedClientPeers());
}

template<typename Transaction, typename Target>
void DiscoveryMonitor::send(ec2::ApiCommand::Value command, Transaction data, const Target& target)
{
    ec2::QnTransaction<Transaction> t(command, commonModule()->moduleGUID(), std::move(data));
    m_messageBus->sendTransaction(std::move(t), target);
}

} // discovery
} // mediaserver
} // nx
