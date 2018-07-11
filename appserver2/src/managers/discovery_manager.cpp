#include "discovery_manager.h"

#include <common/common_module.h>
#include <network/connection_validator.h>
#include <nx_ec/data/api_conversion_functions.h>
#include <transaction/message_bus_adapter.h>
#include <api/global_settings.h>

using namespace nx::vms;

namespace ec2 {

nx::vms::api::DiscoveryData toApiDiscoveryData(
    const QnUuid &id,
    const nx::utils::Url &url,
    bool ignore)
{
    nx::vms::api::DiscoveryData params;
    params.id = id;
    params.url = url.toString();
    params.ignore = ignore;
    return params;
}

nx::vms::api::DiscoveredServerDataList getServers(nx::vms::discovery::Manager* manager)
{
    const auto localSystemId = manager->commonModule()->globalSettings()->localSystemId();

    nx::vms::api::DiscoveredServerDataList result;
    for (const auto& module : manager->getAll())
        result.push_back(makeServer(module, localSystemId));

    return result;
}

nx::vms::api::DiscoveredServerData makeServer(
    const nx::vms::discovery::ModuleEndpoint& module, const QnUuid& localSystemId)
{
    nx::vms::api::DiscoveredServerData serverData(module);
    ec2::setModuleInformationEndpoints(serverData, {module.endpoint});

    if (QnConnectionValidator::validateConnection(module) != Qn::SuccessConnectionResult)
    {
        serverData.status = nx::vms::api::ResourceStatus::incompatible;
    }
    else
    {
        serverData.status = (!localSystemId.isNull() && module.localSystemId == localSystemId)
            ? nx::vms::api::ResourceStatus::online
            : nx::vms::api::ResourceStatus::unauthorized;
    }

    return serverData;
}

QnDiscoveryMonitor::QnDiscoveryMonitor(TransactionMessageBusAdapter* messageBus) :
    QnCommonModuleAware(messageBus->commonModule()),
    m_messageBus(messageBus)
{
    QObject::connect(messageBus, &QnTransactionMessageBus::peerFound,
        this, &QnDiscoveryMonitor::clientFound);

    commonModule()->moduleDiscoveryManager()->onSignals(this, &QnDiscoveryMonitor::serverFound,
        &QnDiscoveryMonitor::serverFound, &QnDiscoveryMonitor::serverLost);
}

QnDiscoveryMonitor::~QnDiscoveryMonitor()
{
    m_messageBus->disconnect(this);
    commonModule()->moduleDiscoveryManager()->disconnect(this);
}

void QnDiscoveryMonitor::clientFound(QnUuid peerId, api::PeerType peerType)
{
    if (!api::PeerData::isClient(peerType))
        return;

    send(ApiCommand::discoveredServersList,
        getServers(commonModule()->moduleDiscoveryManager()), peerId);
}

void QnDiscoveryMonitor::serverFound(nx::vms::discovery::ModuleEndpoint module)
{
    const auto s = makeServer(module, globalSettings()->localSystemId());
    m_serverCache.emplace(s.id, s);
    send(ApiCommand::discoveredServerChanged, s, m_messageBus->directlyConnectedClientPeers());
}

void QnDiscoveryMonitor::serverLost(QnUuid id)
{
    const auto it = m_serverCache.find(id);
    if (it == m_serverCache.end())
        return;

    auto s = it->second;
    m_serverCache.erase(it);

    s.status = nx::vms::api::ResourceStatus::offline;
    send(ApiCommand::discoveredServerChanged, s, m_messageBus->directlyConnectedClientPeers());
}

template<typename Transaction, typename Target>
void QnDiscoveryMonitor::send(ApiCommand::Value command, Transaction data, const Target& target)
{
    QnTransaction<Transaction> t(command, commonModule()->moduleGUID(), std::move(data));
    m_messageBus->sendTransaction(std::move(t), target);
}

} // namespace ec2
