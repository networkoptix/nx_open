#pragma once

#include <transaction/transaction_descriptor.h>

namespace nx::data_sync_engine::command {

struct TranSyncRequest
{
    using Data = vms::api::SyncRequestData;
    static constexpr int code = ::ec2::ApiCommand::tranSyncRequest;

    static QnUuid hash(const Data& data)
    {
        return ::ec2::transactionHash(
            static_cast<::ec2::ApiCommand::Value>(code), data);
    }
};

struct TranSyncResponse
{
    using Data = vms::api::TranStateResponse;
    static constexpr int code = ::ec2::ApiCommand::tranSyncResponse;

    static QnUuid hash(const Data& data)
    {
        return ::ec2::transactionHash(
            static_cast<::ec2::ApiCommand::Value>(code), data);
    }
};

struct TranSyncDone
{
    using Data = vms::api::TranSyncDoneData;
    static constexpr int code = ::ec2::ApiCommand::tranSyncDone;

    static QnUuid hash(const Data& data)
    {
        return ::ec2::transactionHash(
            static_cast<::ec2::ApiCommand::Value>(code), data);
    }
};

} // namespace nx::data_sync_engine::command
