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
    QnTransaction<ApiUpdateData> transaction = prepareTransaction(updateId, data);
    QnTransactionMessageBus::instance()->sendTransaction(transaction, peers);
    QnScopedThreadRollback ensureFreeThread(1);
    QtConcurrent::run(std::bind(&impl::SimpleHandler::done, handler, reqId, ErrorCode::ok));
    return reqId;
}

template<class QueryProcessorType>
int QnUpdatesManager<QueryProcessorType>::installUpdate(const QString &updateId, const PeerList &peers, impl::SimpleHandlerPtr handler) {
    const int reqId = generateRequestID();
    QnTransaction<ApiUpdateData> transaction = prepareTransaction(updateId);
    QnTransactionMessageBus::instance()->sendTransaction(transaction, peers);
    QnScopedThreadRollback ensureFreeThread(1);
    QtConcurrent::run(std::bind(&impl::SimpleHandler::done, handler, reqId, ErrorCode::ok));
    return reqId;
}

template<class QueryProcessorType>
QnTransaction<ApiUpdateData> QnUpdatesManager<QueryProcessorType>::prepareTransaction(const QString &updateId, const QByteArray &data) const {
    QnTransaction<ApiUpdateData> transaction(ApiCommand::uploadUpdate, false);
    transaction.params.updateId = updateId;
    transaction.params.data = data;
    return transaction;
}

template<class QueryProcessorType>
QnTransaction<ApiUpdateData> QnUpdatesManager<QueryProcessorType>::prepareTransaction(const QString &updateId) const {
    QnTransaction<ApiUpdateData> transaction(ApiCommand::installUpdate, false);
    transaction.params.updateId = updateId;
    return transaction;
}

template<class QueryProcessorType>
void QnUpdatesManager<QueryProcessorType>::triggerNotification(const QnTransaction<ApiUpdateData> &transaction) {
    switch (transaction.command) {
    case ApiCommand::uploadUpdate:
        emit updateUploaded(transaction.params.updateId, transaction.params.data);
        break;
    case ApiCommand::installUpdate:
        emit updateInstallationRequested(transaction.params.updateId);
        break;
    default:
        break;
    }
}

template class QnUpdatesManager<ServerQueryProcessor>;
template class QnUpdatesManager<FixedUrlClientQueryProcessor>;

} // namespace ec2
