#include "misc_manager.h"

#include "fixed_url_client_query_processor.h"
#include "server_query_processor.h"
#include "nx_ec/data/api_license_overflow_data.h"

namespace ec2 {

void QnMiscNotificationManager::triggerNotification(const QnTransaction<ApiSystemNameData> &transaction)
{
    emit systemNameChangeRequested(transaction.params.systemName,
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
int QnMiscManager<QueryProcessorType>::changeSystemName(
        const QString &systemName,
        qint64 sysIdTime,
        qint64 tranLogTime,
        impl::SimpleHandlerPtr handler)
{
    const int reqId = generateRequestID();
    auto transaction = prepareTransaction(systemName, sysIdTime, tranLogTime);

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
QnTransaction<ApiSystemNameData> QnMiscManager<QueryProcessorType>::prepareTransaction(
        const QString &systemName,
        qint64 sysIdTime,
        qint64 tranLogTime) const
{
    QnTransaction<ApiSystemNameData> transaction(ApiCommand::changeSystemName);
    transaction.params.systemName = systemName;
    transaction.params.sysIdTime = sysIdTime;
    transaction.params.tranLogTime = tranLogTime;
    return transaction;
}

template<class QueryProcessorType>
int QnMiscManager<QueryProcessorType>::markLicenseOverflow(
        bool value,
        qint64 time,
        impl::SimpleHandlerPtr handler)
{
    const int reqId = generateRequestID();
    QnTransaction<ApiLicenseOverflowData> transaction(ApiCommand::markLicenseOverflow);
    transaction.params.value = value;
    transaction.params.time = time;
    transaction.isLocal = true;

    using namespace std::placeholders;
    m_queryProcessor->getAccess(m_userAccessData).processUpdateAsync(transaction,
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
