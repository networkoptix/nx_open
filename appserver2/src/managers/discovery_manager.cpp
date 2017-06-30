#include "discovery_manager.h"

#include <api/global_settings.h>
#include <common/common_module.h>

#include "fixed_url_client_query_processor.h"
#include "server_query_processor.h"
#include <common/common_module.h>
#include <transaction/message_bus_selector.h>

namespace ec2 {

ApiDiscoveryData toApiDiscoveryData(
    const QnUuid &id,
    const QUrl &url,
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
        manager->checkEndpoint(QUrl(transaction.params.url), transaction.params.id);

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


template<class QueryProcessorType>
QnDiscoveryManager<QueryProcessorType>::QnDiscoveryManager(
    QueryProcessorType * const queryProcessor,
    const Qn::UserAccessData &userAccessData)
:
    m_queryProcessor(queryProcessor),
    m_userAccessData(userAccessData)
{
}

template<class QueryProcessorType>
QnDiscoveryManager<QueryProcessorType>::~QnDiscoveryManager() {}

template<class QueryProcessorType>
int QnDiscoveryManager<QueryProcessorType>::discoverPeer(const QnUuid &id, const QUrl &url, impl::SimpleHandlerPtr handler)
{
    const int reqId = generateRequestID();
    ApiDiscoverPeerData params;
    params.id = id;
    params.url = url.toString();

    using namespace std::placeholders;
    m_queryProcessor->getAccess(m_userAccessData).processUpdateAsync(
        ApiCommand::discoverPeer,
        params,
        [handler, reqId](ErrorCode errorCode)
        {
            handler->done(reqId, errorCode);
        }
    );

    return reqId;
}

template<class QueryProcessorType>
int QnDiscoveryManager<QueryProcessorType>::addDiscoveryInformation(
        const QnUuid &id,
        const QUrl &url,
        bool ignore,
        impl::SimpleHandlerPtr handler)
{
    const int reqId = generateRequestID();
    using namespace std::placeholders;
    m_queryProcessor->getAccess(m_userAccessData).processUpdateAsync(
        ApiCommand::addDiscoveryInformation,
        toApiDiscoveryData(id, url, ignore),
        [handler, reqId](ErrorCode errorCode)
        {
            handler->done(reqId, errorCode);
        }
    );

    return reqId;
}

template<class QueryProcessorType>
int QnDiscoveryManager<QueryProcessorType>::removeDiscoveryInformation(
        const QnUuid &id,
        const QUrl &url,
        bool ignore,
        impl::SimpleHandlerPtr handler)
{
    const int reqId = generateRequestID();
    using namespace std::placeholders;
    m_queryProcessor->getAccess(m_userAccessData).processUpdateAsync(
        ApiCommand::removeDiscoveryInformation,
        toApiDiscoveryData(id, url, ignore),
        [handler, reqId](ErrorCode errorCode)
        {
            handler->done(reqId, errorCode);
        }
    );

    return reqId;
}

template<class QueryProcessorType>
int QnDiscoveryManager<QueryProcessorType>::getDiscoveryData(impl::GetDiscoveryDataHandlerPtr handler)
{
    const int reqID = generateRequestID();

    auto queryDoneHandler = [reqID, handler](ErrorCode errorCode, const ApiDiscoveryDataList &data)
    {
        ApiDiscoveryDataList outData;
        if (errorCode == ErrorCode::ok)
            outData = data;
        handler->done(reqID, errorCode, outData);
    };
    m_queryProcessor->getAccess(m_userAccessData).template processQueryAsync<
            QnUuid, ApiDiscoveryDataList, decltype(queryDoneHandler)>(
                ApiCommand::getDiscoveryData, QnUuid(), queryDoneHandler);
    return reqID;
}

template<class QueryProcessorType>
void QnDiscoveryManager<QueryProcessorType>::monitorServerDiscovery()
{
}

template<>
void QnDiscoveryManager<ServerQueryProcessorAccess>::monitorServerDiscovery()
{
    m_customData = std::make_unique<DiscoveryMonitor>(m_queryProcessor->messageBus());
}

template class QnDiscoveryManager<ServerQueryProcessorAccess>;
template class QnDiscoveryManager<FixedUrlClientQueryProcessor>;

DiscoveryMonitor::DiscoveryMonitor(QnTransactionMessageBusBase* messageBus):
    m_messageBus(messageBus),
    m_common(messageBus->commonModule())
{
    QObject::connect(messageBus, &QnTransactionMessageBus::peerFound,
        this, &DiscoveryMonitor::clientFound);

    m_common->moduleDiscoveryManager()->onSignals(this, &DiscoveryMonitor::serverFound,
        &DiscoveryMonitor::serverFound, &DiscoveryMonitor::serverLost);

    //moveToThread(m_messageBus->thread());
}

DiscoveryMonitor::~DiscoveryMonitor()
{
    m_messageBus->disconnect(this);
    m_common->moduleDiscoveryManager()->disconnect(this);
}

void DiscoveryMonitor::clientFound(QnUuid peerId, Qn::PeerType peerType)
{
    if (!ApiPeerData::isClient(peerType))
        return;

    send(ApiCommand::discoveredServersList,
        getServers(m_common->moduleDiscoveryManager()), peerId);
}

void DiscoveryMonitor::serverFound(nx::vms::discovery::ModuleEndpoint module)
{
    const auto s = makeServer(module, m_common->globalSettings()->localSystemId());
    m_serverCache.emplace(s.id, s);
    send(ApiCommand::discoveredServerChanged, s, m_messageBus->directlyConnectedClientPeers());
}

void DiscoveryMonitor::serverLost(QnUuid id)
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
void DiscoveryMonitor::send(ApiCommand::Value command, Transaction data, const Target& target)
{
    QnTransaction<Transaction> t(command, m_common->moduleGUID(), std::move(data));
    sendTransaction(m_messageBus, std::move(t), target);
}

} // namespace ec2
