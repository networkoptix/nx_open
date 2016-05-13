#include "updates_manager.h"

#include <transaction/transaction_message_bus.h>
#include <utils/common/scoped_thread_rollback.h>
#include <utils/common/concurrent.h>

#include "ec2_thread_pool.h"
#include "fixed_url_client_query_processor.h"
#include "server_query_processor.h"


namespace ec2 {


    ////////////////////////////////////////////////////////////
    //// class QnUpdatesNotificationManager
    ////////////////////////////////////////////////////////////
    QnUpdatesNotificationManager::QnUpdatesNotificationManager()
    {
    }

    void QnUpdatesNotificationManager::triggerNotification(const QnTransaction<ApiUpdateUploadData> &transaction) {
        NX_ASSERT(transaction.command == ApiCommand::uploadUpdate);
        emit updateChunkReceived(transaction.params.updateId, transaction.params.data, transaction.params.offset);
    }

    void QnUpdatesNotificationManager::triggerNotification(const QnTransaction<ApiUpdateUploadResponceData> &transaction) {
        NX_ASSERT(transaction.command == ApiCommand::uploadUpdateResponce);
        emit updateUploadProgress(transaction.params.updateId, transaction.params.id, transaction.params.chunks);
    }

    void QnUpdatesNotificationManager::triggerNotification(const QnTransaction<ApiUpdateInstallData> &transaction) {
        NX_ASSERT(transaction.command == ApiCommand::installUpdate);
        //TODO: #2.4 #rvasilenko #dklychkov Implement a mechanism to determine the transaction was successfully sent to the other peers, then emit this signal.
        emit updateInstallationRequested(transaction.params.updateId);
    }



    ////////////////////////////////////////////////////////////
    //// class QnUpdatesManager
    ////////////////////////////////////////////////////////////


    template<class QueryProcessorType>
    QnUpdatesManager<QueryProcessorType>::QnUpdatesManager(QueryProcessorType * const queryProcessor) :
        m_queryProcessor(queryProcessor)
    {
    }

    template<class QueryProcessorType>
    QnUpdatesManager<QueryProcessorType>::~QnUpdatesManager() {}

    template<class QueryProcessorType>
    int QnUpdatesManager<QueryProcessorType>::sendUpdatePackageChunk(const QString &updateId, const QByteArray &data, qint64 offset, const QnPeerSet &peers, impl::SimpleHandlerPtr handler) {
        const int reqId = generateRequestID();
        auto transaction = prepareTransaction(updateId, data, offset);

        QnTransactionMessageBus::instance()->sendTransaction(transaction, peers);
        QnConcurrent::run( Ec2ThreadPool::instance(),[handler, reqId](){ handler->done(reqId, ErrorCode::ok); });

        return reqId;
    }

    template<class QueryProcessorType>
    int QnUpdatesManager<QueryProcessorType>::sendUpdateUploadResponce(const QString &updateId, const QnUuid &peerId, int chunks, impl::SimpleHandlerPtr handler) {
        const int reqId = generateRequestID();
        auto transaction = prepareTransaction(updateId, peerId, chunks);

        using namespace std::placeholders;
        m_queryProcessor->processUpdateAsync(transaction, [handler, reqId](ErrorCode errorCode){ handler->done(reqId, errorCode); });

        return reqId;
    }

    template<class QueryProcessorType>
    int QnUpdatesManager<QueryProcessorType>::installUpdate(const QString &updateId, const QnPeerSet &peers, impl::SimpleHandlerPtr handler) {
        const int reqId = generateRequestID();
        auto transaction = prepareTransaction(updateId);

        QnTransactionMessageBus::instance()->sendTransaction(transaction, peers);
        QnConcurrent::run( Ec2ThreadPool::instance(),[handler, reqId](){ handler->done(reqId, ErrorCode::ok); });

        return reqId;
    }

    template<class QueryProcessorType>
    QnTransaction<ApiUpdateUploadData> QnUpdatesManager<QueryProcessorType>::prepareTransaction(const QString &updateId, const QByteArray &data, qint64 offset) const {
        QnTransaction<ApiUpdateUploadData> transaction(ApiCommand::uploadUpdate);
        transaction.params.updateId = updateId;
        transaction.params.data = data;
        transaction.params.offset = offset;
        return transaction;
    }

    template<class QueryProcessorType>
    QnTransaction<ApiUpdateUploadResponceData> QnUpdatesManager<QueryProcessorType>::prepareTransaction(const QString &updateId, const QnUuid &peerId, int chunks) const {
        QnTransaction<ApiUpdateUploadResponceData> transaction(ApiCommand::uploadUpdateResponce);
        transaction.params.id = peerId;
        transaction.params.updateId = updateId;
        transaction.params.chunks = chunks;
        return transaction;
    }

    template<class QueryProcessorType>
    QnTransaction<ApiUpdateInstallData> QnUpdatesManager<QueryProcessorType>::prepareTransaction(const QString &updateId) const {
        QnTransaction<ApiUpdateInstallData> transaction(ApiCommand::installUpdate);
        transaction.params = updateId;
        return transaction;
    }

    template class QnUpdatesManager<ServerQueryProcessorAccess>;
    template class QnUpdatesManager<FixedUrlClientQueryProcessor>;

} // namespace ec2
