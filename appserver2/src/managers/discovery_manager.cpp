#include "discovery_manager.h"

#include "utils/network/module_finder.h"
#include "utils/network/direct_module_finder.h"
#include "fixed_url_client_query_processor.h"
#include "server_query_processor.h"

namespace ec2 {

void QnDiscoveryNotificationManager::triggerNotification(const QnTransaction<ApiDiscoverPeerData> &transaction) {
    Q_ASSERT_X(transaction.command == ApiCommand::discoverPeer, "Invalid command for this function", Q_FUNC_INFO);

    // TODO: maybe it's better to move it out and use signal?..
    QnModuleFinder *moduleFinder = QnModuleFinder::instance();
    if (moduleFinder && moduleFinder->directModuleFinder())
        moduleFinder->directModuleFinder()->checkUrl(QUrl(transaction.params.url));

//    emit peerDiscoveryRequested(QUrl(transaction.params.url));
}

void QnDiscoveryNotificationManager::triggerNotification(const QnTransaction<ApiDiscoveryDataList> &transaction) {
    Q_ASSERT_X(transaction.command == ApiCommand::addDiscoveryInformation || transaction.command == ApiCommand::removeDiscoveryInformation, "Invalid command for this function", Q_FUNC_INFO);

    triggerNotification(transaction.params, transaction.command == ApiCommand::addDiscoveryInformation);
}

void QnDiscoveryNotificationManager::triggerNotification(const ApiDiscoveryDataList &discoveryData, bool addInformation) {
    emit discoveryInformationChanged(discoveryData, addInformation);
}


template<class QueryProcessorType>
QnDiscoveryManager<QueryProcessorType>::QnDiscoveryManager(QueryProcessorType * const queryProcessor) :
    m_queryProcessor(queryProcessor)
{
}

template<class QueryProcessorType>
QnDiscoveryManager<QueryProcessorType>::~QnDiscoveryManager() {}

template<class QueryProcessorType>
int QnDiscoveryManager<QueryProcessorType>::discoverPeer(const QUrl &url, impl::SimpleHandlerPtr handler) {
    const int reqId = generateRequestID();
    auto transaction = prepareTransaction(url);

    using namespace std::placeholders;
    m_queryProcessor->processUpdateAsync(transaction, [handler, reqId](ErrorCode errorCode){ handler->done(reqId, errorCode); });

    return reqId;
}

template<class QueryProcessorType>
int QnDiscoveryManager<QueryProcessorType>::addDiscoveryInformation(const QnUuid &id, const QList<QUrl> &urls, bool ignore, impl::SimpleHandlerPtr handler) {
    const int reqId = generateRequestID();
    auto transaction = prepareTransaction(ApiCommand::addDiscoveryInformation, id, urls, ignore);

    using namespace std::placeholders;
    m_queryProcessor->processUpdateAsync(transaction, [handler, reqId](ErrorCode errorCode){ handler->done(reqId, errorCode); });

    return reqId;
}

template<class QueryProcessorType>
int QnDiscoveryManager<QueryProcessorType>::removeDiscoveryInformation(const QnUuid &id, const QList<QUrl> &urls, bool ignore, impl::SimpleHandlerPtr handler) {
    const int reqId = generateRequestID();
    auto transaction = prepareTransaction(ApiCommand::removeDiscoveryInformation, id, urls, ignore);

    using namespace std::placeholders;
    m_queryProcessor->processUpdateAsync(transaction, [handler, reqId](ErrorCode errorCode){ handler->done(reqId, errorCode); });

    return reqId;
}

template<class QueryProcessorType>
QnTransaction<ApiDiscoveryDataList> QnDiscoveryManager<QueryProcessorType>::prepareTransaction(ApiCommand::Value command, const QnUuid &id, const QList<QUrl> &urls, bool ignore) const {
    QnTransaction<ApiDiscoveryDataList> transaction(command);
    foreach (const QUrl &url, urls) {
        Q_ASSERT_X(ignore || url.port() != -1, "Additional server URLs without a port are not allowed", Q_FUNC_INFO);
        ApiDiscoveryData data;
        data.id = id;
        data.url = url.toString();
        data.ignore = ignore;
        transaction.params.push_back(data);
    }

    return transaction;
}

template<class QueryProcessorType>
QnTransaction<ApiDiscoverPeerData> QnDiscoveryManager<QueryProcessorType>::prepareTransaction(const QUrl &url) const {
    QnTransaction<ApiDiscoverPeerData> transaction(ApiCommand::discoverPeer);
    transaction.params.url = url.toString();

    return transaction;
}

template class QnDiscoveryManager<ServerQueryProcessor>;
template class QnDiscoveryManager<FixedUrlClientQueryProcessor>;

} // namespace ec2
