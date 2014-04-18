#include "updates_manager.h"

#include <QtConcurrent>

#include <transaction/transaction_message_bus.h>
#include <utils/common/scoped_thread_rollback.h>
#include "fixed_url_client_query_processor.h"
#include "server_query_processor.h"

namespace ec2 {

template<class QueryProcessorType>
QnUpdatesManager<QueryProcessorType>::QnUpdatesManager(QueryProcessorType * const queryProcessor) :
    m_queryProcessor(queryProcessor)
{}

template<class QueryProcessorType>
QnUpdatesManager<QueryProcessorType>::~QnUpdatesManager() {}

template<class QueryProcessorType>
int QnUpdatesManager<QueryProcessorType>::sendUpdatePackage(const QString &updateId, const QByteArray &data, const PeerList &peers, impl::SimpleHandlerPtr handler) {
    const int reqId = generateRequestID();
    auto transaction = prepareTransaction(updateId, data);

    QnTransactionMessageBus::instance()->sendTransaction(transaction, peers);
    QnScopedThreadRollback ensureFreeThread(1);
    QtConcurrent::run(std::bind(&impl::SimpleHandler::done, handler, reqId, ErrorCode::ok));

    return reqId;
}

template<class QueryProcessorType>
int QnUpdatesManager<QueryProcessorType>::sendUpdateUploadedResponce(const QString &updateId, const QnId &peerId, impl::SimpleHandlerPtr handler) {
    const int reqId = generateRequestID();
    auto transaction = prepareTransaction(updateId, peerId);

    using namespace std::placeholders;
    m_queryProcessor->processUpdateAsync(transaction, std::bind(&impl::SimpleHandler::done, handler, reqId, _1));

    return reqId;
}

template<class QueryProcessorType>
int QnUpdatesManager<QueryProcessorType>::installUpdate(const QString &updateId, impl::SimpleHandlerPtr handler) {
    const int reqId = generateRequestID();
    auto transaction = prepareTransaction(updateId);

    using namespace std::placeholders;
    m_queryProcessor->processUpdateAsync(transaction, std::bind(&impl::SimpleHandler::done, handler, reqId, _1));

    return reqId;
}

template<class QueryProcessorType>
QnTransaction<ApiUpdateUploadData> QnUpdatesManager<QueryProcessorType>::prepareTransaction(const QString &updateId, const QByteArray &data) const {
    QnTransaction<ApiUpdateUploadData> transaction(ApiCommand::uploadUpdate, false);
    transaction.params.updateId = updateId;
    transaction.params.data = data;
    return transaction;
}

template<class QueryProcessorType>
QnTransaction<ApiUpdateUploadResponceData> QnUpdatesManager<QueryProcessorType>::prepareTransaction(const QString &updateId, const QnId &peerId) const {
    QnTransaction<ApiUpdateUploadResponceData> transaction(ApiCommand::uploadUpdateResponce, true);
    transaction.params.id = peerId;
    transaction.params.updateId = updateId;
    return transaction;
}

template<class QueryProcessorType>
QnTransaction<ApiUpdateInstallData> QnUpdatesManager<QueryProcessorType>::prepareTransaction(const QString &updateId) const {
    QnTransaction<ApiUpdateInstallData> transaction(ApiCommand::installUpdate, true);
    transaction.params = updateId;
    return transaction;
}

template<class QueryProcessorType>
void QnUpdatesManager<QueryProcessorType>::triggerNotification(const QnTransaction<ApiUpdateUploadData> &transaction) {
    assert(transaction.command == ApiCommand::uploadUpdate);
    emit updateReceived(transaction.params.updateId, transaction.params.data);
}

template<class QueryProcessorType>
void QnUpdatesManager<QueryProcessorType>::triggerNotification(const QnTransaction<ApiUpdateUploadResponceData> &transaction) {
    assert(transaction.command == ApiCommand::uploadUpdateResponce);
    emit updateUploaded(transaction.params.updateId, transaction.params.id);
}

template<class QueryProcessorType>
void QnUpdatesManager<QueryProcessorType>::triggerNotification(const QnTransaction<ApiUpdateInstallData> &transaction) {
    assert(transaction.command == ApiCommand::installUpdate);
    emit updateInstallationRequested(transaction.params);
}

template class QnUpdatesManager<ServerQueryProcessor>;
template class QnUpdatesManager<FixedUrlClientQueryProcessor>;

} // namespace ec2
