#include "discovery_manager.h"

#include "utils/network/module_finder.h"
#include "utils/network/direct_module_finder.h"
#include "fixed_url_client_query_processor.h"
#include "server_query_processor.h"

namespace ec2 {

template<class QueryProcessorType>
QnDiscoveryNotificationManager<QueryProcessorType>::QnDiscoveryNotificationManager(QueryProcessorType * const queryProcessor) :
    m_queryProcessor(queryProcessor)
{
}

template<class QueryProcessorType>
QnDiscoveryNotificationManager<QueryProcessorType>::~QnDiscoveryNotificationManager() {}

template<class QueryProcessorType>
void QnDiscoveryNotificationManager<QueryProcessorType>::triggerNotification(const QnTransaction<ApiDiscoverPeerData> &transaction) {
    Q_ASSERT_X(transaction.command == ApiCommand::discoverPeer, "Invalid command for this function", Q_FUNC_INFO);

    // TODO: maybe it's better to move it out and use signal?..
    QnModuleFinder *moduleFinder = QnModuleFinder::instance();
    if (moduleFinder && moduleFinder->directModuleFinder())
        moduleFinder->directModuleFinder()->checkUrl(QUrl(transaction.params.url));

//    emit peerDiscoveryRequested(QUrl(transaction.params.url));
}

template<class QueryProcessorType>
void QnDiscoveryNotificationManager<QueryProcessorType>::triggerNotification(const QnTransaction<ApiDiscoveryDataList> &transaction) {
    Q_ASSERT_X(transaction.command == ApiCommand::addDiscoveryInformation || transaction.command == ApiCommand::removeDiscoveryInformation, "Invalid command for this function", Q_FUNC_INFO);

    triggerNotification(transaction.params, transaction.command == ApiCommand::addDiscoveryInformation);
}

template<class QueryProcessorType>
void QnDiscoveryNotificationManager<QueryProcessorType>::triggerNotification(const ApiDiscoveryDataList &discoveryData, bool addInformation) {
    emit discoveryInformationChanged(discoveryData, addInformation);
}

template<class QueryProcessorType>
int QnDiscoveryNotificationManager<QueryProcessorType>::discoverPeer(const QUrl &url, impl::SimpleHandlerPtr handler) {
    const int reqId = generateRequestID();
    auto transaction = prepareTransaction(url);

    using namespace std::placeholders;
    m_queryProcessor->processUpdateAsync(transaction, [handler, reqId](ErrorCode errorCode){ handler->done(reqId, errorCode); });

    return reqId;
}

template<class QueryProcessorType>
int QnDiscoveryNotificationManager<QueryProcessorType>::addDiscoveryInformation(const QnId &id, const QList<QUrl> &urls, bool ignore, impl::SimpleHandlerPtr handler) {
    const int reqId = generateRequestID();
    auto transaction = prepareTransaction(ApiCommand::addDiscoveryInformation, id, urls, ignore);

    using namespace std::placeholders;
    m_queryProcessor->processUpdateAsync(transaction, [handler, reqId](ErrorCode errorCode){ handler->done(reqId, errorCode); });

    return reqId;
}

template<class QueryProcessorType>
int QnDiscoveryNotificationManager<QueryProcessorType>::removeDiscoveryInformation(const QnId &id, const QList<QUrl> &urls, bool ignore, impl::SimpleHandlerPtr handler) {
    const int reqId = generateRequestID();
    auto transaction = prepareTransaction(ApiCommand::removeDiscoveryInformation, id, urls, ignore);

    using namespace std::placeholders;
    m_queryProcessor->processUpdateAsync(transaction, [handler, reqId](ErrorCode errorCode){ handler->done(reqId, errorCode); });

    return reqId;
}

template<class QueryProcessorType>
QnTransaction<ApiDiscoveryDataList> QnDiscoveryNotificationManager<QueryProcessorType>::prepareTransaction(ApiCommand::Value command, const QnId &id, const QList<QUrl> &urls, bool ignore) const {
    QnTransaction<ApiDiscoveryDataList> transaction(command, true);
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
QnTransaction<ApiDiscoverPeerData> QnDiscoveryNotificationManager<QueryProcessorType>::prepareTransaction(const QUrl &url) const {
    QnTransaction<ApiDiscoverPeerData> transaction(ApiCommand::discoverPeer, false);
    transaction.params.url = url.toString();

    return transaction;
}

template class QnDiscoveryNotificationManager<ServerQueryProcessor>;
template class QnDiscoveryNotificationManager<FixedUrlClientQueryProcessor>;

} // namespace ec2
