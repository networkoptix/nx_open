// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "abstract_misc_manager.h"

#include "../detail/call_sync.h"

namespace ec2 {

ErrorCode AbstractMiscManager::changeSystemIdSync(
    const nx::Uuid& systemId, qint64 sysIdTime, nx::vms::api::Timestamp tranLogTime)
{
    return detail::callSync(
        [&](auto handler)
        {
            changeSystemId(systemId, sysIdTime, tranLogTime, std::move(handler));
        });
}

ErrorCode AbstractMiscManager::cleanupDatabaseSync(
    bool cleanupDbObjects, bool cleanupTransactionLog)
{
    return detail::callSync(
        [&](auto handler)
        {
            cleanupDatabase(cleanupDbObjects, cleanupTransactionLog, std::move(handler));
        });
}

ErrorCode AbstractMiscManager::markLicenseOverflowSync(bool value, qint64 time)
{
    return detail::callSync(
        [&](auto handler)
        {
            markLicenseOverflow(value, time, std::move(handler));
        });
}

ErrorCode AbstractMiscManager::markVideoWallLicenseOverflowSync(bool value, qint64 time)
{
    return detail::callSync(
        [&](auto handler)
        {
            markVideoWallLicenseOverflow(value, time, std::move(handler));
        });
}

ErrorCode AbstractMiscManager::saveMiscParamSync(const nx::vms::api::MiscData& data)
{
    return detail::callSync(
        [&](auto handler)
        {
            saveMiscParam(data, std::move(handler));
        });
}

ErrorCode AbstractMiscManager::getMiscParamSync(
    const QByteArray& paramName, nx::vms::api::MiscData* outData)
{
    return detail::callSync(
        [&](auto handler)
        {
            getMiscParam(paramName, std::move(handler));
        },
        outData);
}

ErrorCode AbstractMiscManager::saveRuntimeInfoSync(const nx::vms::api::RuntimeData& data)
{
    return detail::callSync(
        [&](auto handler)
        {
            saveRuntimeInfo(data, std::move(handler));
        });
}

Result AbstractMiscManager::saveSystemMergeHistoryRecordSync(
    const nx::vms::api::SystemMergeHistoryRecord& record)
{
    return detail::callSync(
        [&](auto handler)
        {
            saveSystemMergeHistoryRecord(record, std::move(handler));
        });
}

ErrorCode AbstractMiscManager::getSystemMergeHistorySync(
    nx::vms::api::SystemMergeHistoryRecordList* outData)
{
    return detail::callSync(
        [&](auto handler)
        {
            getSystemMergeHistory(std::move(handler));
        },
        outData);
}

} // namespace ec2
