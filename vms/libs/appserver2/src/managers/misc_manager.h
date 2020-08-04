#pragma once

#include <nx_ec/ec_api.h>
#include <transaction/transaction.h>

#include <nx/vms/api/data/license_overflow_data.h>
#include <nx/vms/api/data/videowall_license_overflow_data.h>
#include <nx/vms/api/data/system_id_data.h>

namespace ec2 {

template<class QueryProcessorType>
class QnMiscManager: public AbstractMiscManager
{
public:
    QnMiscManager(QueryProcessorType* const queryProcessor,
        const Qn::UserSession& userSession);
    virtual ~QnMiscManager();

    virtual int markLicenseOverflow(
        bool value, qint64 time, impl::SimpleHandlerPtr handler) override;
    virtual int markVideoWallLicenseOverflow(
        bool value, qint64 time, impl::SimpleHandlerPtr handler) override;
    virtual int cleanupDatabase(bool cleanupDbObjects, bool cleanupTransactionLog,
        impl::SimpleHandlerPtr handler) override;

    virtual int saveMiscParam(
        const nx::vms::api::MiscData& params, impl::SimpleHandlerPtr handler) override;
    virtual int getMiscParam(
        const QByteArray& paramName, impl::GetMiscParamHandlerPtr handler) override;

    virtual int saveSystemMergeHistoryRecord(const nx::vms::api::SystemMergeHistoryRecord& param,
        impl::SimpleHandlerPtr handler) override;
    virtual int getSystemMergeHistory(impl::GetSystemMergeHistoryHandlerPtr handler) override;

    virtual int saveRuntimeInfo(const nx::vms::api::RuntimeData& data,
        impl::SimpleHandlerPtr handler) override;

protected:
    virtual int changeSystemId(const QnUuid& systemId, qint64 sysIdTime,
        nx::vms::api::Timestamp tranLogTime, impl::SimpleHandlerPtr handler) override;

private:
    QueryProcessorType* const m_queryProcessor;
    Qn::UserSession m_userSession;
};

template<class QueryProcessorType>
QnMiscManager<QueryProcessorType>::QnMiscManager(
    QueryProcessorType * const queryProcessor,
    const Qn::UserSession& userSession)
    :
    m_queryProcessor(queryProcessor),
    m_userSession(userSession)
{
}

template<class QueryProcessorType>
QnMiscManager<QueryProcessorType>::~QnMiscManager() {}

template<class QueryProcessorType>
int QnMiscManager<QueryProcessorType>::changeSystemId(
        const QnUuid& systemId,
        qint64 sysIdTime,
        nx::vms::api::Timestamp tranLogTime,
        impl::SimpleHandlerPtr handler)
{
    const int reqId = generateRequestID();

    nx::vms::api::SystemIdData params;
    params.systemId = systemId;
    params.sysIdTime = sysIdTime;
    params.tranLogTime = tranLogTime;

    using namespace std::placeholders;
    m_queryProcessor->getAccess(m_userSession).processUpdateAsync(
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
    nx::vms::api::LicenseOverflowData params;
    params.value = value;
    params.time = time;

    using namespace std::placeholders;
    m_queryProcessor->getAccess(m_userSession).processUpdateAsync(
        ApiCommand::markLicenseOverflow, params,
        [handler, reqId, params](ErrorCode errorCode)
        {
            handler->done(reqId, errorCode);
        }
    );

    return reqId;
}

template<class QueryProcessorType>
int QnMiscManager<QueryProcessorType>::markVideoWallLicenseOverflow(
        bool value,
        qint64 time,
        impl::SimpleHandlerPtr handler)
{
    const int reqId = generateRequestID();
    nx::vms::api::VideoWallLicenseOverflowData params;
    params.value = value;
    params.time = time;

    using namespace std::placeholders;
    m_queryProcessor->getAccess(m_userSession).processUpdateAsync(
        ApiCommand::markVideoWallLicenseOverflow, params,
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
    nx::vms::api::CleanupDatabaseData data;
    data.cleanupDbObjects = cleanupDbObjects;
    data.cleanupTransactionLog = cleanupTransactionLog;

    using namespace std::placeholders;
    m_queryProcessor->getAccess(m_userSession).processUpdateAsync(
        ApiCommand::cleanupDatabase,
        data,
        [handler, reqId](ErrorCode errorCode)
        {
            handler->done(reqId, errorCode);
        });

    return reqId;
}

template<class T>
int QnMiscManager<T>::saveMiscParam(
    const nx::vms::api::MiscData& param, impl::SimpleHandlerPtr handler)
{
    const int reqID = generateRequestID();
    m_queryProcessor->getAccess(m_userSession).processUpdateAsync(
        ApiCommand::saveMiscParam, param,
        [handler, reqID](ec2::ErrorCode errorCode)
    {
        handler->done(reqID, errorCode);
    });
    return reqID;
}

template<class T>
int QnMiscManager<T>::saveRuntimeInfo(const nx::vms::api::RuntimeData& data,
    impl::SimpleHandlerPtr handler)
{
    const int reqID = generateRequestID();
    m_queryProcessor->getAccess(m_userSession).processUpdateAsync(
        ApiCommand::runtimeInfoChanged, data,
        [handler, reqID](ec2::ErrorCode errorCode)
    {
        handler->done(reqID, errorCode);
    });
    return reqID;
}

template<class T>
int QnMiscManager<T>::getMiscParam(
    const QByteArray& paramName, impl::GetMiscParamHandlerPtr handler)
{
    const int reqID = generateRequestID();
    const auto queryDoneHandler =
        [reqID, handler, paramName](ErrorCode errorCode, const nx::vms::api::MiscData& param)
        {
            nx::vms::api::MiscData outData;
            if (errorCode == ErrorCode::ok)
                outData = param;
            handler->done(reqID, errorCode, outData);
        };

    m_queryProcessor->getAccess(m_userSession).template processQueryAsync<
            QByteArray, nx::vms::api::MiscData, decltype(queryDoneHandler)>
        (ApiCommand::getMiscParam, paramName, queryDoneHandler);

    return reqID;
}

template<class T>
int QnMiscManager<T>::saveSystemMergeHistoryRecord(
    const nx::vms::api::SystemMergeHistoryRecord& data,
    impl::SimpleHandlerPtr handler)
{
    const int reqID = generateRequestID();
    m_queryProcessor->getAccess(m_userSession).processUpdateAsync(
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
            const nx::vms::api::SystemMergeHistoryRecordList& outData)
        {
            if (errorCode == ErrorCode::ok)
                handler->done(reqID, errorCode, std::move(outData));
            else
                handler->done(reqID, errorCode, nx::vms::api::SystemMergeHistoryRecordList());
        };

    m_queryProcessor->getAccess(m_userSession).template processQueryAsync<std::nullptr_t /*dummy*/,
            nx::vms::api::SystemMergeHistoryRecordList, decltype(queryDoneHandler)>
        (ApiCommand::getSystemMergeHistory, nullptr, queryDoneHandler);

    return reqID;
}

} // namespace ec2
