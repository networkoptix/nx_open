#include "misc_manager.h"

#include "fixed_url_client_query_processor.h"
#include "server_query_processor.h"
#include "nx_ec/data/api_license_overflow_data.h"

namespace ec2 {

void QnMiscNotificationManager::triggerNotification(
    const QnTransaction<ApiSystemIdData> &transaction,
    NotificationSource /*source*/)
{
    emit systemIdChangeRequested(transaction.params.systemId,
                                   transaction.params.sysIdTime,
                                   transaction.params.tranLogTime);
}

void QnMiscNotificationManager::triggerNotification(const QnTransaction<ApiMiscData> &transaction)
{
    emit miscDataChanged(transaction.params.name, transaction.params.value);
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
        [handler, reqId, params](ErrorCode errorCode)
        {
            handler->done(reqId, errorCode);
        }
    );

    return reqId;
}

template<class QueryProcessorType>
int QnMiscManager<QueryProcessorType>::cleanupDatabase(
    bool cleanupDbObjects,
    bool cleanupTransactionLog,
    impl::SimpleHandlerPtr handler)
{
    const int reqId = generateRequestID();
    ApiCleanupDatabaseData data;
    data.cleanupDbObjects = cleanupDbObjects;
    data.cleanupTransactionLog = cleanupTransactionLog;

    using namespace std::placeholders;
    m_queryProcessor->getAccess(m_userAccessData).processUpdateAsync(
        ApiCommand::cleanupDatabase,
	    data,
        [handler, reqId](ErrorCode errorCode)
        {
            handler->done(reqId, errorCode);
        });

    return reqId;
}

template<class T>
int QnMiscManager<T>::saveMiscParam(const ec2::ApiMiscData& param, impl::SimpleHandlerPtr handler)
{
    const int reqID = generateRequestID();
    m_queryProcessor->getAccess(m_userAccessData).processUpdateAsync(
        ApiCommand::saveMiscParam, param,
        [handler, reqID](ec2::ErrorCode errorCode)
    {
        handler->done(reqID, errorCode);
    });
    return reqID;
}

template<class T>
int QnMiscManager<T>::saveRuntimeInfo(const ec2::ApiRuntimeData& data, impl::SimpleHandlerPtr handler)
{
    const int reqID = generateRequestID();
    m_queryProcessor->getAccess(m_userAccessData).processUpdateAsync(
        ApiCommand::runtimeInfoChanged, data,
        [handler, reqID](ec2::ErrorCode errorCode)
    {
        handler->done(reqID, errorCode);
    });
    return reqID;
}

template<class T>
int QnMiscManager<T>::getMiscParam(const QByteArray& paramName, impl::GetMiscParamHandlerPtr handler)
{
    const int reqID = generateRequestID();

    auto queryDoneHandler = [reqID, handler, paramName](ErrorCode errorCode, const ApiMiscData& param)
    {
        ApiMiscData outData;
        if (errorCode == ErrorCode::ok)
            outData = param;
        handler->done(reqID, errorCode, outData);
    };
    m_queryProcessor->getAccess(m_userAccessData).template processQueryAsync<QByteArray, ApiMiscData, decltype(queryDoneHandler)>
        (ApiCommand::getMiscParam, paramName, queryDoneHandler);
    return reqID;
}

template<class T>
int QnMiscManager<T>::saveSystemMergeHistoryRecord(
    const ApiSystemMergeHistoryRecord& data,
    impl::SimpleHandlerPtr handler)
{
    const int reqID = generateRequestID();
    m_queryProcessor->getAccess(m_userAccessData).processUpdateAsync(
        ApiCommand::saveSystemMergeHistoryRecord,
        data,
        [handler, reqID](ec2::ErrorCode errorCode)
        {
            handler->done(reqID, errorCode);
        });
    return reqID;
}

template<class T>
int QnMiscManager<T>::getSystemMergeHistory(
    impl::GetSystemMergeHistoryHandlerPtr handler)
{
    const int reqID = generateRequestID();

    auto queryDoneHandler =
        [reqID, handler](
            ErrorCode errorCode,
            const ApiSystemMergeHistoryRecordList& outData)
        {
            if (errorCode == ErrorCode::ok)
                handler->done(reqID, errorCode, std::move(outData));
            else
                handler->done(reqID, errorCode, ApiSystemMergeHistoryRecordList());
        };
    m_queryProcessor->getAccess(m_userAccessData)
        .template processQueryAsync<QByteArray /*dummy*/, ApiSystemMergeHistoryRecordList, decltype(queryDoneHandler)>
            (ApiCommand::getSystemMergeHistory, QByteArray(), queryDoneHandler);
    return reqID;
}

template class QnMiscManager<ServerQueryProcessorAccess>;
template class QnMiscManager<FixedUrlClientQueryProcessor>;

} // namespace ec2
