#pragma once

#include <string>

#include <transaction/transaction.h>
#include <transaction/transaction_transport_header.h>

namespace nx::clusterdb::engine {

using CommandHeader = ::ec2::QnAbstractTransaction;

NX_DATA_SYNC_ENGINE_API std::string toString(const CommandHeader& header);

template<typename Data>
class Command:
    public ::ec2::QnTransaction<Data>
{
    using base_type = ::ec2::QnTransaction<Data>;

public:
    Command() = default;

    Command(CommandHeader header):
        base_type(std::move(header))
    {
    }

    Command(int commandCode, QnUuid peerId):
        base_type(
            static_cast<::ec2::ApiCommand::Value>(commandCode),
            std::move(peerId))
    {
    }

    void setHeader(CommandHeader commandHeader)
    {
        // TODO: #ak Not good, but I cannot change basic ec2 classes.
        static_cast<CommandHeader&>(*this) = std::move(commandHeader);
    }
};

// TODO: #ak Remove this type.
using VmsTransportHeader = ::ec2::QnTransactionTransportHeader;

} // namespace nx::clusterdb::engine
