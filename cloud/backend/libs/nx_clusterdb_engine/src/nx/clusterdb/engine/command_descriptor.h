#pragma once

#include <transaction/transaction_descriptor.h>

#include "command.h"

namespace nx::clusterdb::engine::command {

template<typename CommandDescriptor>
struct CommandDataHashFunctor
{
    nx::Buffer operator()(const typename CommandDescriptor::Data& data)
    {
        return ::ec2::transactionHash(
            static_cast<::ec2::ApiCommand::Value>(CommandDescriptor::code),
            data).toSimpleByteArray();
    }
};

template<typename DataType, int commandCode>
struct BaseCommandDescriptor
{
    using Data = DataType;
    static constexpr int code = commandCode;
    using HashFunctor =
        CommandDataHashFunctor<BaseCommandDescriptor<DataType, commandCode>>;

    static nx::Buffer hash(const Data& data)
    {
        return HashFunctor()(data);
    }
};

//-------------------------------------------------------------------------------------------------

struct TranSyncRequest:
    BaseCommandDescriptor<
        vms::api::SyncRequestData,
        ::ec2::ApiCommand::tranSyncRequest>
{
    static constexpr char name[] = "tranSyncRequest";
};

struct TranSyncResponse:
    BaseCommandDescriptor<
        vms::api::TranStateResponse,
        ::ec2::ApiCommand::tranSyncResponse>
{
    static constexpr char name[] = "tranSyncResponse";
};

struct TranSyncDone:
    BaseCommandDescriptor<
        vms::api::TranSyncDoneData,
        ::ec2::ApiCommand::tranSyncDone>
{
    static constexpr char name[] = "tranSyncDone";
};

struct UpdatePersistentSequence:
    BaseCommandDescriptor<
        vms::api::UpdateSequenceData,
        ::ec2::ApiCommand::updatePersistentSequence>
{
    static constexpr char name[] = "updatePersistentSequence";
};

//-------------------------------------------------------------------------------------------------

template<typename CommandDescriptor>
Command<typename CommandDescriptor::Data> make(
    const QnUuid& sourcePeerId)
{
    return Command<typename CommandDescriptor::Data>(
        static_cast<::ec2::ApiCommand::Value>(CommandDescriptor::code),
        sourcePeerId);
}

static constexpr int kUserCommand = 20000;

} // namespace nx::clusterdb::engine::command
