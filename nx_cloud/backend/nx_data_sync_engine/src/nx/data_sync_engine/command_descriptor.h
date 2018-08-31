#pragma once

#include <transaction/transaction_descriptor.h>

namespace nx::data_sync_engine::command {

struct TranSyncRequest
{
    using Data = vms::api::SyncRequestData;
    static constexpr int code = ::ec2::ApiCommand::tranSyncRequest;
    static constexpr char name[] = "tranSyncRequest";

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
    static constexpr char name[] = "tranSyncResponse";

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
    static constexpr char name[] = "tranSyncDone";

    static QnUuid hash(const Data& data)
    {
        return ::ec2::transactionHash(
            static_cast<::ec2::ApiCommand::Value>(code), data);
    }
};

struct UpdatePersistentSequence
{
    using Data = vms::api::UpdateSequenceData;
    static constexpr int code = ::ec2::ApiCommand::updatePersistentSequence;
    static constexpr char name[] = "updatePersistentSequence";

    static QnUuid hash(const Data& data)
    {
        return ::ec2::transactionHash(
            static_cast<::ec2::ApiCommand::Value>(code), data);
    }
};

template<typename CommandDescriptor>
Command<typename CommandDescriptor::Data> make(
    const QnUuid& sourcePeerId)
{
    return Command<typename CommandDescriptor::Data>(
        static_cast<::ec2::ApiCommand::Value>(CommandDescriptor::code),
        sourcePeerId);
}

} // namespace nx::data_sync_engine::command
