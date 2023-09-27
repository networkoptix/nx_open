// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/api/data/cleanup_db_data.h>
#include <nx/vms/api/data/license_overflow_data.h>
#include <nx/vms/api/data/system_id_data.h>
#include <nx/vms/api/data/videowall_license_overflow_data.h>
#include <nx_ec/managers/abstract_misc_manager.h>
#include <transaction/transaction.h>

namespace ec2 {

template<class QueryProcessorType>
class QnMiscManager: public AbstractMiscManager
{
public:
    QnMiscManager(QueryProcessorType* queryProcessor, const Qn::UserSession& userSession);

    virtual int changeSystemId(
        const QnUuid& systemId,
        qint64 sysIdTime,
        nx::vms::api::Timestamp tranLogTime,
        Handler<> handler,
        nx::utils::AsyncHandlerExecutor handlerExecutor = {}) override;

    virtual int cleanupDatabase(
        bool cleanupDbObjects,
        bool cleanupTransactionLog,
        Handler<> handler,
        nx::utils::AsyncHandlerExecutor handlerExecutor = {}) override;

    virtual int markLicenseOverflow(
        bool value,
        qint64 time,
        Handler<> handler,
        nx::utils::AsyncHandlerExecutor handlerExecutor = {}) override;

    virtual int markVideoWallLicenseOverflow(
        bool value,
        qint64 time,
        Handler<> handler,
        nx::utils::AsyncHandlerExecutor handlerExecutor = {}) override;

    virtual int saveMiscParam(
        const nx::vms::api::MiscData& param,
        Handler<> handler,
        nx::utils::AsyncHandlerExecutor handlerExecutor = {}) override;

    virtual int getMiscParam(
        const QByteArray& paramName,
        Handler<nx::vms::api::MiscData> handler,
        nx::utils::AsyncHandlerExecutor handlerExecutor = {}) override;

    virtual int saveRuntimeInfo(
        const nx::vms::api::RuntimeData& data,
        Handler<> handler,
        nx::utils::AsyncHandlerExecutor handlerExecutor = {}) override;

    virtual int saveSystemMergeHistoryRecord(
        const nx::vms::api::SystemMergeHistoryRecord& record,
        Handler<> handler,
        nx::utils::AsyncHandlerExecutor handlerExecutor = {}) override;

    virtual int getSystemMergeHistory(
        Handler<nx::vms::api::SystemMergeHistoryRecordList> handler,
        nx::utils::AsyncHandlerExecutor handlerExecutor = {}) override;

private:
    decltype(auto) processor() { return m_queryProcessor->getAccess(m_userSession); }

private:
    QueryProcessorType* const m_queryProcessor;
    Qn::UserSession m_userSession;
};

template<class QueryProcessorType>
QnMiscManager<QueryProcessorType>::QnMiscManager(
    QueryProcessorType* queryProcessor, const Qn::UserSession& userSession)
    :
    m_queryProcessor(queryProcessor),
    m_userSession(userSession)
{
}

template<class QueryProcessorType>
int QnMiscManager<QueryProcessorType>::changeSystemId(
    const QnUuid& systemId,
    qint64 sysIdTime,
    nx::vms::api::Timestamp tranLogTime,
    Handler<> handler,
    nx::utils::AsyncHandlerExecutor handlerExecutor)
{
    nx::vms::api::SystemIdData params;
    params.systemId = systemId;
    params.sysIdTime = sysIdTime;
    params.tranLogTime = tranLogTime;

    const int requestId = generateRequestID();
    processor().processUpdateAsync(
        ApiCommand::changeSystemId,
        params,
        [requestId, handler = handlerExecutor.bind(std::move(handler))](auto&&... args) mutable
        {
            handler(requestId, std::move(args)...);
        });
    return requestId;
}

template<class QueryProcessorType>
int QnMiscManager<QueryProcessorType>::cleanupDatabase(
    bool cleanupDbObjects,
    bool cleanupTransactionLog,
    Handler<> handler,
    nx::utils::AsyncHandlerExecutor handlerExecutor)
{
    nx::vms::api::CleanupDatabaseData data;
    data.cleanupDbObjects = cleanupDbObjects;
    data.cleanupTransactionLog = cleanupTransactionLog;

    const int requestId = generateRequestID();
    processor().processUpdateAsync(
        ApiCommand::cleanupDatabase,
        data,
        [requestId, handler = handlerExecutor.bind(std::move(handler))](auto&&... args) mutable
        {
            handler(requestId, std::move(args)...);
        });
    return requestId;
}

template<class QueryProcessorType>
int QnMiscManager<QueryProcessorType>::markLicenseOverflow(
    bool value,
    qint64 time,
    Handler<> handler,
    nx::utils::AsyncHandlerExecutor handlerExecutor)
{
    nx::vms::api::LicenseOverflowData params;
    params.value = value;
    params.time = time;

    const int requestId = generateRequestID();
    processor().processUpdateAsync(
        ApiCommand::markLicenseOverflow,
        params,
        [requestId, handler = handlerExecutor.bind(std::move(handler))](auto&&... args) mutable
        {
            handler(requestId, std::move(args)...);
        });
    return requestId;
}

template<class QueryProcessorType>
int QnMiscManager<QueryProcessorType>::markVideoWallLicenseOverflow(
    bool value,
    qint64 time,
    Handler<> handler,
    nx::utils::AsyncHandlerExecutor handlerExecutor)
{
    nx::vms::api::VideoWallLicenseOverflowData params;
    params.value = value;
    params.time = time;

    const int requestId = generateRequestID();
    processor().processUpdateAsync(
        ApiCommand::markVideoWallLicenseOverflow,
        params,
        [requestId, handler = handlerExecutor.bind(std::move(handler))](auto&&... args) mutable
        {
            handler(requestId, std::move(args)...);
        });
    return requestId;
}

template<class T>
int QnMiscManager<T>::saveMiscParam(
    const nx::vms::api::MiscData& param,
    Handler<> handler,
    nx::utils::AsyncHandlerExecutor handlerExecutor)
{
    const int requestId = generateRequestID();
    processor().processUpdateAsync(
        ApiCommand::saveMiscParam,
        param,
        [requestId, handler = handlerExecutor.bind(std::move(handler))](auto&&... args) mutable
        {
            handler(requestId, std::move(args)...);
        });
    return requestId;
}

template<class T>
int QnMiscManager<T>::getMiscParam(
    const QByteArray& paramName,
    Handler<nx::vms::api::MiscData> handler,
    nx::utils::AsyncHandlerExecutor handlerExecutor)
{
    const int requestId = generateRequestID();
    processor().template processQueryAsync<QByteArray, nx::vms::api::MiscData>(
        ApiCommand::getMiscParam,
        paramName,
        [requestId, handler = handlerExecutor.bind(std::move(handler))](auto&&... args) mutable
        {
            handler(requestId, std::move(args)...);
        });
    return requestId;
}

template<class T>
int QnMiscManager<T>::saveRuntimeInfo(
    const nx::vms::api::RuntimeData& data,
    Handler<> handler,
    nx::utils::AsyncHandlerExecutor handlerExecutor)
{
    const int requestId = generateRequestID();
    processor().processUpdateAsync(
        ApiCommand::runtimeInfoChanged,
        data,
        [requestId, handler = handlerExecutor.bind(std::move(handler))](auto&&... args) mutable
        {
            handler(requestId, std::move(args)...);
        });
    return requestId;
}

template<class T>
int QnMiscManager<T>::saveSystemMergeHistoryRecord(
    const nx::vms::api::SystemMergeHistoryRecord& record,
    Handler<> handler,
    nx::utils::AsyncHandlerExecutor handlerExecutor)
{
    const int requestId = generateRequestID();
    processor().processUpdateAsync(
        ApiCommand::saveSystemMergeHistoryRecord,
        record,
        [requestId, handler = handlerExecutor.bind(std::move(handler))](auto&&... args) mutable
        {
            handler(requestId, std::move(args)...);
        });
    return requestId;
}

template<class T>
int QnMiscManager<T>::getSystemMergeHistory(
    Handler<nx::vms::api::SystemMergeHistoryRecordList> handler,
    nx::utils::AsyncHandlerExecutor handlerExecutor)
{
    using namespace nx::vms::api;
    const int requestId = generateRequestID();
    processor().template processQueryAsync<std::nullptr_t, SystemMergeHistoryRecordList>(
        ApiCommand::getSystemMergeHistory,
        nullptr,
        [requestId, handler = handlerExecutor.bind(std::move(handler))](auto&&... args) mutable
        {
            handler(requestId, std::move(args)...);
        });
    return requestId;
}

} // namespace ec2
