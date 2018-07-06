#include "discovery_manager.h"

#include <common/common_module.h>
#include <transaction/message_bus_adapter.h>
#include <api/global_settings.h>

namespace ec2 {

ApiDiscoveryData toApiDiscoveryData(
    const QnUuid &id,
    const nx::utils::Url &url,
    bool ignore)
{
    ApiDiscoveryData params;
    params.id = id;
    params.url = url.toString();
    params.ignore = ignore;
    return params;
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

void QnDiscoveryMonitor::clientFound(QnUuid peerId, Qn::PeerType peerType)
{
    if (!ApiPeerData::isClient(peerType))
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
