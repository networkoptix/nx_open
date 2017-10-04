#include "discovery_manager.h"

#include <api/global_settings.h>
#include <common/common_module.h>

#include "fixed_url_client_query_processor.h"
#include "server_query_processor.h"
#include <common/common_module.h>
#include <transaction/message_bus_adapter.h>

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
int QnDiscoveryManager<QueryProcessorType>::discoverPeer(
    const QnUuid &id,
    const nx::utils::Url& url,
    impl::SimpleHandlerPtr handler)
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
        const nx::utils::Url &url,
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
    const nx::utils::Url &url,
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

template class QnDiscoveryManager<ServerQueryProcessorAccess>;
template class QnDiscoveryManager<FixedUrlClientQueryProcessor>;

QnDiscoveryMonitor::QnDiscoveryMonitor(TransactionMessageBusAdapter* messageBus):
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
