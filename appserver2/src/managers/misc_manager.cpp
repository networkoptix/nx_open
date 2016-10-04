#include "misc_manager.h"

#include "fixed_url_client_query_processor.h"
#include "server_query_processor.h"
#include "nx_ec/data/api_license_overflow_data.h"

namespace ec2 {

void QnMiscNotificationManager::triggerNotification(const QnTransaction<ApiSystemIdData> &transaction)
{
    emit systemIdChangeRequested(transaction.params.systemId,
                                   transaction.params.sysIdTime,
                                   transaction.params.tranLogTime);
}

template<class QueryProcessorType>
QnMiscManager<QueryProcessorType>::QnMiscManager(QueryProcessorType * const queryProcessor,
                                                 const Qn::UserAccessData &userAccessData) :
    m_queryProcessor(queryProcessor),
    m_userAccessData(userAccessData)
{
}

template<class QueryProcessorType>
QnMiscManager<QueryProcessorType>::~QnMiscManager() {}

template<class QueryProcessorType>
int QnMiscManager<QueryProcessorType>::changeSystemId(
        const QnUuid& systemId,
        qint64 sysIdTime,
        Timestamp tranLogTime,
        impl::SimpleHandlerPtr handler)
{
    const int reqId = generateRequestID();


    ApiSystemIdData params;
    params.systemId = systemId;
    params.sysIdTime = sysIdTime;
    params.tranLogTime = tranLogTime;

    using namespace std::placeholders;
    m_queryProcessor->getAccess(m_userAccessData).processUpdateAsync(
        ApiCommand::changeSystemId, params,
        [handler, reqId](ErrorCode errorCode)
        {
            handler->done(reqId, errorCode);
        }
    );

    return reqId;
}

template<class QueryProcessorType>
int QnMiscManager<QueryProcessorType>::markLicenseOverflow(
        bool value,
        qint64 time,
        impl::SimpleHandlerPtr handler)
{
    const int reqId = generateRequestID();
    ApiLicenseOverflowData params;
    params.value = value;
    params.time = time;

    using namespace std::placeholders;
    m_queryProcessor->getAccess(m_userAccessData).processUpdateAsync(
        ApiCommand::markLicenseOverflow, params,
        [handler, reqId](ErrorCode errorCode)
        {
            handler->done(reqId, errorCode);
        }
    );

    return reqId;
}

template<class QueryProcessorType>
int QnMiscManager<QueryProcessorType>::rebuildTransactionLog(
    impl::SimpleHandlerPtr handler)
{
    const int reqId = generateRequestID();
    ApiRebuildTransactionLogData data;

    using namespace std::placeholders;
    m_queryProcessor->getAccess(m_userAccessData).processUpdateAsync(ApiCommand::rebuildTransactionLog, data,
        [handler, reqId](ErrorCode errorCode)
    {
        handler->done(reqId, errorCode);
    }
    );

    return reqId;
}

template class QnMiscManager<ServerQueryProcessorAccess>;
template class QnMiscManager<FixedUrlClientQueryProcessor>;

} // namespace ec2
