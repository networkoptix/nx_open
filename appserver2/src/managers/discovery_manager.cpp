#include "discovery_manager.h"

#include <common/common_module.h>

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

QnDiscoveryNotificationManager::QnDiscoveryNotificationManager(QnCommonModule* commonModule):
    QnCommonModuleAware(commonModule)
{
}

void QnDiscoveryNotificationManager::triggerNotification(const QnTransaction<ApiDiscoverPeerData> &transaction, NotificationSource /*source*/)
{
    NX_ASSERT(transaction.command == ApiCommand::discoverPeer, "Invalid command for this function", Q_FUNC_INFO);

    // TODO: maybe it's better to move it out and use signal?..
    if (const auto manager = commonModule()->moduleDiscoveryManager())
        manager->checkEndpoint(nx::utils::Url(transaction.params.url), transaction.params.id);

//    emit peerDiscoveryRequested(QUrl(transaction.params.url));
}

void QnDiscoveryNotificationManager::triggerNotification(const QnTransaction<ApiDiscoveryData> &transaction, NotificationSource /*source*/)
{
    NX_ASSERT(transaction.command == ApiCommand::addDiscoveryInformation ||
               transaction.command == ApiCommand::removeDiscoveryInformation,
               Q_FUNC_INFO, "Invalid command for this function");

    triggerNotification(transaction.params, transaction.command == ApiCommand::addDiscoveryInformation);
}

void QnDiscoveryNotificationManager::triggerNotification(const ApiDiscoveryData &discoveryData, bool addInformation)
{
    emit discoveryInformationChanged(discoveryData, addInformation);
}

void QnDiscoveryNotificationManager::triggerNotification(const QnTransaction<ApiDiscoveryDataList> &tran, NotificationSource /*source*/)
{
    for (const ApiDiscoveryData &data: tran.params)
        emit discoveryInformationChanged(data, true);
}

void QnDiscoveryNotificationManager::triggerNotification(const QnTransaction<ApiDiscoveredServerData> &tran, NotificationSource /*source*/)
{
    emit discoveredServerChanged(tran.params);
}

void QnDiscoveryNotificationManager::triggerNotification(const QnTransaction<ApiDiscoveredServerDataList> &tran, NotificationSource /*source*/)
{
    emit gotInitialDiscoveredServers(tran.params);
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

    s.status = Qn::ResourceStatus::Offline;
    send(ApiCommand::discoveredServerChanged, s, m_messageBus->directlyConnectedClientPeers());
}

template<typename Transaction, typename Target>
void QnDiscoveryMonitor::send(ApiCommand::Value command, Transaction data, const Target& target)
{
    QnTransaction<Transaction> t(command, commonModule()->moduleGUID(), std::move(data));
    m_messageBus->sendTransaction(std::move(t), target);
}

} // namespace ec2
