#include "discovery_manager.h"

#include "core/resource_management/resource_pool.h"
#include "core/resource/media_server_resource.h"
#include "utils/network/module_finder.h"
#include "utils/network/direct_module_finder.h"
#include "fixed_url_client_query_processor.h"
#include "server_query_processor.h"

namespace ec2 {

template<class QueryProcessorType>
QnDiscoveryManager<QueryProcessorType>::QnDiscoveryManager(QueryProcessorType * const queryProcessor) :
    m_queryProcessor(queryProcessor)
{
}

template<class QueryProcessorType>
QnDiscoveryManager<QueryProcessorType>::~QnDiscoveryManager() {}

template<class QueryProcessorType>
void QnDiscoveryManager<QueryProcessorType>::triggerNotification(const QnTransaction<ApiDiscoverPeerData> &transaction) {
    Q_ASSERT_X(transaction.command == ApiCommand::discoverPeer, "Invalid command for this function", Q_FUNC_INFO);

    // TODO: maybe it's better to move it out and use signal?..
    QnModuleFinder *moduleFinder = QnModuleFinder::instance();
    if (moduleFinder)
        moduleFinder->directModuleFinder()->checkUrl(QUrl(transaction.params.url));

//    emit peerDiscoveryRequested(QUrl(transaction.params.url));
}

template<class QueryProcessorType>
void QnDiscoveryManager<QueryProcessorType>::triggerNotification(const QnTransaction<ApiDiscoveryDataList> &transaction) {
    Q_ASSERT_X(transaction.command == ApiCommand::addDiscoveryInformation || transaction.command == ApiCommand::removeDiscoveryInformation, "Invalid command for this function", Q_FUNC_INFO);

    QHash<QnId, QUrl> m_additionalUrls;
    QHash<QnId, QUrl> m_ignoredUrls;

    foreach (const ApiDiscoveryData &data, transaction.params) {
        if (data.ignore)
            m_ignoredUrls.insert(data.id, data.url);
        else
            m_additionalUrls.insert(data.id, data.url);
    }

    foreach (const QnId &id, m_additionalUrls.uniqueKeys()) {
        QnMediaServerResourcePtr server = qnResPool->getResourceById(id).dynamicCast<QnMediaServerResource>();
        if (!server)
            continue;

        if (transaction.command == ApiCommand::addDiscoveryInformation)
            server->setAdditionalUrls(m_additionalUrls.values(id));
    }

    foreach (const QnId &id, m_ignoredUrls.uniqueKeys()) {
        QnMediaServerResourcePtr server = qnResPool->getResourceById(id).dynamicCast<QnMediaServerResource>();
        if (!server)
            continue;

        if (transaction.command == ApiCommand::addDiscoveryInformation)
            server->setIgnoredUrls(m_additionalUrls.values(id));
    }
}

template<class QueryProcessorType>
int QnDiscoveryManager<QueryProcessorType>::discoverPeer(const QUrl &url, impl::SimpleHandlerPtr handler) {
    const int reqId = generateRequestID();
    auto transaction = prepareTransaction(url);

    using namespace std::placeholders;
    m_queryProcessor->processUpdateAsync(transaction, [handler, reqId](ErrorCode errorCode){ handler->done(reqId, errorCode); });

    return reqId;
}

template<class QueryProcessorType>
int QnDiscoveryManager<QueryProcessorType>::addDiscoveryInformation(const QnId &id, const QList<QUrl> &urls, bool ignore, impl::SimpleHandlerPtr handler) {

}

template<class QueryProcessorType>
int QnDiscoveryManager<QueryProcessorType>::removeDiscoveryInformation(const QnId &id, const QList<QUrl> &urls, bool ignore, impl::SimpleHandlerPtr handler) {

}

template<class QueryProcessorType>
QnTransaction<ApiDiscoveryDataList> QnDiscoveryManager<QueryProcessorType>::prepareTransaction(ApiCommand::Value command, const ApiDiscoveryDataList &discoveryDataList) const {
    QnTransaction<ApiDiscoveryDataList> transaction(command, true);
    transaction.params = discoveryDataList;

    return transaction;
}

template<class QueryProcessorType>
QnTransaction<ApiDiscoverPeerData> QnDiscoveryManager<QueryProcessorType>::prepareTransaction(const QUrl &url) const {
    QnTransaction<ApiDiscoverPeerData> transaction(ApiCommand::discoverPeer, false);
    transaction.params.url = url.toString();

    return transaction;
}

template class QnDiscoveryManager<ServerQueryProcessor>;
template class QnDiscoveryManager<FixedUrlClientQueryProcessor>;

} // namespace ec2
