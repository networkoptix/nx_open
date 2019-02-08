#pragma once

#include "utils/db/db_helper.h"
#include "transaction/transaction.h"
#include "ec2_thread_pool.h"

#include <nx/utils/concurrent.h>
#include <nx/vms/api/data/update_data.h>

namespace ec2 {

    template<class QueryProcessorType>
    class QnUpdatesManager
    :
        public AbstractUpdatesManager
    {
    public:
        QnUpdatesManager(
            QueryProcessorType * const queryProcessor,
            const Qn::UserAccessData &userAccessData,
            TransactionMessageBusAdapter* messageBus);
        virtual ~QnUpdatesManager();

    protected:
        virtual int sendUpdatePackageChunk(
            const QString &updateId, const QByteArray &data, qint64 offset,
            const nx::vms::api::PeerSet &peers, impl::SimpleHandlerPtr handler) override;

        virtual int sendUpdateUploadResponce(const QString &updateId, const QnUuid &peerId,
            int chunks, impl::SimpleHandlerPtr handler) override;

    private:
        QueryProcessorType* const m_queryProcessor;
        Qn::UserAccessData m_userAccessData;
        TransactionMessageBusAdapter* m_messageBus;
    };

    ////////////////////////////////////////////////////////////
    //// class QnUpdatesManager
    ////////////////////////////////////////////////////////////


    template<class QueryProcessorType>
    QnUpdatesManager<QueryProcessorType>::QnUpdatesManager(
        QueryProcessorType * const queryProcessor,
        const Qn::UserAccessData &userAccessData,
        TransactionMessageBusAdapter* messageBus)
    :
        m_queryProcessor(queryProcessor),
        m_userAccessData(userAccessData),
        m_messageBus(messageBus)
    {
    }

    template<class QueryProcessorType>
    QnUpdatesManager<QueryProcessorType>::~QnUpdatesManager() {}

    template<class QueryProcessorType>
    int QnUpdatesManager<QueryProcessorType>::sendUpdatePackageChunk(
        const QString &updateId,
        const QByteArray &data,
        qint64 offset,
        const nx::vms::api::PeerSet& peers,
        impl::SimpleHandlerPtr handler)
    {
        const int reqId = generateRequestID();
        nx::vms::api::UpdateUploadData params;
        params.updateId = updateId;
        params.data = data;
        params.offset = offset;

        m_queryProcessor->getAccess(m_userAccessData).processUpdateAsync(
            ApiCommand::uploadUpdate, params,
            [handler, reqId](ErrorCode errorCode) { handler->done(reqId, errorCode); });

        return reqId;
    }

    template<class QueryProcessorType>
    int QnUpdatesManager<QueryProcessorType>::sendUpdateUploadResponce(const QString& updateId,
        const QnUuid& peerId, int chunks, impl::SimpleHandlerPtr handler)
    {
        const int reqId = generateRequestID();
        nx::vms::api::UpdateUploadResponseData params;
        params.id = peerId;
        params.updateId = updateId;
        params.chunks = chunks;

        m_queryProcessor->getAccess(m_userAccessData).processUpdateAsync(
            ApiCommand::uploadUpdateResponce, params,
            [handler, reqId](ErrorCode errorCode){ handler->done(reqId, errorCode); });

        return reqId;
    }

} // namespace ec2
