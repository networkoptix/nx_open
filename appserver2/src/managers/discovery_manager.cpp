#include "discovery_manager.h"

#include "network/module_finder.h"
#include "network/direct_module_finder.h"
#include "fixed_url_client_query_processor.h"
#include "server_query_processor.h"

namespace ec2 {

void QnDiscoveryNotificationManager::triggerNotification(const QnTransaction<ApiDiscoverPeerData> &transaction)
{
    NX_ASSERT(transaction.command == ApiCommand::discoverPeer, "Invalid command for this function", Q_FUNC_INFO);

    // TODO: maybe it's better to move it out and use signal?..
    QnModuleFinder *moduleFinder = QnModuleFinder::instance();
    if (moduleFinder && moduleFinder->directModuleFinder())
        moduleFinder->directModuleFinder()->checkUrl(QUrl(transaction.params.url));

//    emit peerDiscoveryRequested(QUrl(transaction.params.url));
}

void QnDiscoveryNotificationManager::triggerNotification(const QnTransaction<ApiDiscoveryData> &transaction)
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

void QnDiscoveryNotificationManager::triggerNotification(const QnTransaction<ApiDiscoveryDataList> &tran)
{
    for (const ApiDiscoveryData &data: tran.params)
        emit discoveryInformationChanged(data, true);
}

void QnDiscoveryNotificationManager::triggerNotification(const QnTransaction<ApiDiscoveredServerData> &tran)
{
    emit discoveredServerChanged(tran.params);
}

void QnDiscoveryNotificationManager::triggerNotification(const QnTransaction<ApiDiscoveredServerDataList> &tran)
{
    emit gotInitialDiscoveredServers(tran.params);
}


template<class QueryProcessorType>
QnDiscoveryManager<QueryProcessorType>::QnDiscoveryManager(QnDiscoveryNotificationManagerRawPtr *base, QueryProcessorType * const queryProcessor, const Qn::UserAccessData &userAccessData) :
    m_base(base),
    m_queryProcessor(queryProcessor),
    m_userAccessData(userAccessData)
{
}

template<class QueryProcessorType>
QnDiscoveryManager<QueryProcessorType>::~QnDiscoveryManager() {}

template<class QueryProcessorType>
int QnDiscoveryManager<QueryProcessorType>::discoverPeer(const QUrl &url, impl::SimpleHandlerPtr handler)
{
    const int reqId = generateRequestID();
    auto transaction = prepareTransaction(url);

    using namespace std::placeholders;
    m_queryProcessor->getAccess(m_userAccessData).processUpdateAsync(transaction,
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
    auto transaction = prepareTransaction(ApiCommand::addDiscoveryInformation, id, url, ignore);

    using namespace std::placeholders;
    m_queryProcessor->getAccess(m_userAccessData).processUpdateAsync(transaction,
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
    auto transaction = prepareTransaction(ApiCommand::removeDiscoveryInformation, id, url, ignore);

    using namespace std::placeholders;
    m_queryProcessor->getAccess(m_userAccessData).processUpdateAsync(transaction,
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
            std::nullptr_t, ApiDiscoveryDataList, decltype(queryDoneHandler)>(
                ApiCommand::getDiscoveryData, nullptr, queryDoneHandler);
    return reqID;
}

template<class QueryProcessorType>
int QnDiscoveryManager<QueryProcessorType>::sendDiscoveredServer(
        const ApiDiscoveredServerData &discoveredServer,
        impl::SimpleHandlerPtr handler)
{
    const int reqId = generateRequestID();
    auto transaction = prepareTransaction(discoveredServer);

    using namespace std::placeholders;
    m_queryProcessor->getAccess(m_userAccessData).processUpdateAsync(transaction,
        [handler, reqId](ErrorCode errorCode)
        {
            handler->done(reqId, errorCode);
        }
    );

    return reqId;
}

template<class QueryProcessorType>
int QnDiscoveryManager<QueryProcessorType>::sendDiscoveredServersList(
        const ApiDiscoveredServerDataList &discoveredServersList,
        impl::SimpleHandlerPtr handler)
{
    const int reqId = generateRequestID();
    auto transaction = prepareTransaction(discoveredServersList);

    using namespace std::placeholders;
    m_queryProcessor->getAccess(m_userAccessData).processUpdateAsync(transaction,
        [handler, reqId](ErrorCode errorCode)
        {
            handler->done(reqId, errorCode);
        }
    );

    return reqId;
}

template<class QueryProcessorType>
QnTransaction<ApiDiscoveryData> QnDiscoveryManager<QueryProcessorType>::prepareTransaction(
        ApiCommand::Value command,
        const QnUuid &id,
        const QUrl &url,
        bool ignore) const
{
    QnTransaction<ApiDiscoveryData> transaction(command);
    transaction.params.id = id;
    transaction.params.url = url.toString();
    transaction.params.ignore = ignore;

    return transaction;
}

template<class QueryProcessorType>
QnTransaction<ApiDiscoverPeerData> QnDiscoveryManager<QueryProcessorType>::prepareTransaction(
        const QUrl &url) const
{
    QnTransaction<ApiDiscoverPeerData> transaction(ApiCommand::discoverPeer);
    transaction.params.url = url.toString();

    return transaction;
}

template<class QueryProcessorType>
QnTransaction<ApiDiscoveredServerData> QnDiscoveryManager<QueryProcessorType>::prepareTransaction(
        const ApiDiscoveredServerData &discoveredServer) const
{
    QnTransaction<ApiDiscoveredServerData> transaction(ApiCommand::discoveredServerChanged);
    transaction.params = discoveredServer;

    return transaction;
}

template<class QueryProcessorType>
QnTransaction<ApiDiscoveredServerDataList> QnDiscoveryManager<QueryProcessorType>::prepareTransaction(
        const ApiDiscoveredServerDataList &discoveredServersList) const
{
    QnTransaction<ApiDiscoveredServerDataList> transaction(ApiCommand::discoveredServersList);
    transaction.params = discoveredServersList;

    return transaction;
}

template class QnDiscoveryManager<ServerQueryProcessorAccess>;
template class QnDiscoveryManager<FixedUrlClientQueryProcessor>;

} // namespace ec2
