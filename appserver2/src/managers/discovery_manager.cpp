#include "discovery_manager.h"

#include <api/global_settings.h>
#include <common/common_module.h>
#include <nx/vms/discovery/manager.h>

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
    const auto messageBus = m_queryProcessor->messageBus();
    QObject::connect(messageBus, &QnTransactionMessageBus::peerFound,
        [messageBus](QnUuid peerId, Qn::PeerType peerType)
        {
            if (!ApiPeerData::isClient(peerType))
                return;

            const auto servers = getServers(messageBus->commonModule()->moduleDiscoveryManager());
            QnTransaction<ApiDiscoveredServerDataList> transaction(
                ApiCommand::discoveredServersList, messageBus->commonModule()->moduleGUID(), servers);

            sendTransaction(messageBus, transaction, peerId);
        });

    const auto updateServerStatus =
        [messageBus](nx::vms::discovery::ModuleEndpoint module)
        {
            const auto peers = messageBus->directlyConnectedClientPeers();
            if (peers.isEmpty())
                return;

            QnTransaction<ApiDiscoveredServerData> transaction(
                ApiCommand::discoveredServerChanged, messageBus->commonModule()->moduleGUID(),
                makeServer(module, messageBus->commonModule()->globalSettings()->localSystemId()));

            sendTransaction(messageBus, transaction, peers);
        };

    const auto discoveryManager = messageBus->commonModule()->moduleDiscoveryManager();
    QObject::connect(discoveryManager, &nx::vms::discovery::Manager::found, updateServerStatus);
    QObject::connect(discoveryManager, &nx::vms::discovery::Manager::changed, updateServerStatus);
    // TODO: nx::vms::discovery::Manager::lost
}

template class QnDiscoveryManager<ServerQueryProcessorAccess>;
template class QnDiscoveryManager<FixedUrlClientQueryProcessor>;

} // namespace ec2
