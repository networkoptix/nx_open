// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <nx/utils/async_handler_executor.h>
#include <nx/utils/uuid.h>
#include <nx/vms/api/data/misc_data.h>
#include <nx/vms/api/data/license_overflow_data.h>
#include <nx/vms/api/data/resource_data.h>
#include <nx/vms/api/data/runtime_data.h>
#include <nx/vms/api/data/system_merge_history_record.h>
#include <nx/vms/api/data/timestamp.h>

#include "../ec_api_common.h"

namespace ec2 {

class NX_VMS_COMMON_API AbstractMiscNotificationManager: public QObject
{
    Q_OBJECT

signals:
    void systemIdChangeRequested(
        const nx::Uuid& systemId, qint64 sysIdTime, nx::vms::api::Timestamp tranLogTime);

    void miscDataChanged(const QString& name, const QString& value);
};

class NX_VMS_COMMON_API AbstractMiscManager
{
public:
    virtual ~AbstractMiscManager() = default;

    virtual int changeSystemId(
        const nx::Uuid& systemId,
        qint64 sysIdTime,
        nx::vms::api::Timestamp tranLogTime,
        Handler<> handler,
        nx::utils::AsyncHandlerExecutor handlerExecutor = {}) = 0;

    ErrorCode changeSystemIdSync(
        const nx::Uuid& systemId, qint64 sysIdTime, nx::vms::api::Timestamp tranLogTime);

    virtual int cleanupDatabase(
        bool cleanupDbObjects,
        bool cleanupTransactionLog,
        Handler<> handler,
        nx::utils::AsyncHandlerExecutor handlerExecutor = {}) = 0;

    ErrorCode cleanupDatabaseSync(bool cleanupDbObjects, bool cleanupTransactionLog);

    virtual int markLicenseOverflow(
        std::chrono::milliseconds expirationTimeMs,
        nx::vms::api::GracePeriodType type,
        Handler<> handler,
        nx::utils::AsyncHandlerExecutor handlerExecutor = {}) = 0;

    ErrorCode markLicenseOverflowSync(
        std::chrono::milliseconds expirationTimeMs,
        nx::vms::api::GracePeriodType type);

    virtual int saveMiscParam(
        const nx::vms::api::MiscData& param,
        Handler<> handler,
        nx::utils::AsyncHandlerExecutor handlerExecutor = {}) = 0;

    ErrorCode saveMiscParamSync(const nx::vms::api::MiscData& data);

    virtual int getMiscParam(
        const QByteArray& paramName,
        Handler<nx::vms::api::MiscData> handler,
        nx::utils::AsyncHandlerExecutor handlerExecutor = {}) = 0;

    ErrorCode getMiscParamSync(
        const QByteArray& paramName, nx::vms::api::MiscData* outData);

    virtual int saveRuntimeInfo(
        const nx::vms::api::RuntimeData& data,
        Handler<> handler,
        nx::utils::AsyncHandlerExecutor handlerExecutor = {}) = 0;

    ErrorCode saveRuntimeInfoSync(const nx::vms::api::RuntimeData& data);

    virtual int saveSystemMergeHistoryRecord(
        const nx::vms::api::SystemMergeHistoryRecord& record,
        Handler<> handler,
        nx::utils::AsyncHandlerExecutor handlerExecutor = {}) = 0;

    Result saveSystemMergeHistoryRecordSync(
        const nx::vms::api::SystemMergeHistoryRecord& record);

    virtual int getSystemMergeHistory(
        Handler<nx::vms::api::SystemMergeHistoryRecordList> handler,
        nx::utils::AsyncHandlerExecutor handlerExecutor = {}) = 0;

    ErrorCode getSystemMergeHistorySync(nx::vms::api::SystemMergeHistoryRecordList* outData);

    virtual int updateKvPairsWithLowPriority(
        nx::vms::api::ResourceParamWithRefDataList params,
        Handler<> handler,
        nx::utils::AsyncHandlerExecutor handlerExecutor = {}) = 0;
    ErrorCode updateKvPairsWithLowPrioritySync(nx::vms::api::ResourceParamWithRefDataList params);

};

} // namespace ec2
