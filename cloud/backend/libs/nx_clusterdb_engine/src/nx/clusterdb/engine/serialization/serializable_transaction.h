#pragma once

#include "transaction_serializer.h"
#include "../command.h"

namespace nx::clusterdb::engine {

class NX_DATA_SYNC_ENGINE_API SerializableAbstractCommand:
    public CommandSerializer
{
public:
    virtual const CommandHeader& header() const = 0;
};

//-------------------------------------------------------------------------------------------------

class NX_DATA_SYNC_ENGINE_API EditableSerializableCommand:
    public SerializableAbstractCommand
{
public:
    virtual std::unique_ptr<EditableSerializableCommand> clone() const = 0;
    virtual void setHeader(CommandHeader header) = 0;
    virtual nx::Buffer hash() const = 0;
};

//-------------------------------------------------------------------------------------------------

template<typename CommandDescriptor>
class SerializableCommand:
    public EditableSerializableCommand
{
public:
    SerializableCommand(Command<typename CommandDescriptor::Data> transaction):
        m_transaction(std::move(transaction))
    {
    }

    virtual nx::Buffer serialize(
        Qn::SerializationFormat targetFormat,
        int /*transactionFormatVersion*/) const override
    {
        //NX_ASSERT(transactionFormatVersion == nx_ec::EC2_PROTO_VERSION);

        switch (targetFormat)
        {
            case Qn::UbjsonFormat:
                return QnUbjson::serialized(m_transaction);

            default:
                NX_ASSERT(false);
                return nx::Buffer();
        }
    }

    virtual nx::Buffer serialize(
        Qn::SerializationFormat targetFormat,
        const CommandTransportHeader& transportHeader,
        int /*transactionFormatVersion*/) const override
    {
        //NX_ASSERT(transactionFormatVersion == nx_ec::EC2_PROTO_VERSION);

        switch (targetFormat)
        {
            case Qn::UbjsonFormat:
                return QnUbjson::serialized(transportHeader.vmsTransportHeader) +
                    QnUbjson::serialized(m_transaction);

            default:
                NX_ASSERT(false);
                return nx::Buffer();
        }
    }

    virtual const CommandHeader& header() const override
    {
        return m_transaction;
    }

    virtual std::unique_ptr<EditableSerializableCommand> clone() const override
    {
        return std::make_unique<SerializableCommand<CommandDescriptor>>(
            m_transaction);
    }

    virtual void setHeader(CommandHeader header) override
    {
        m_transaction.setHeader(std::move(header));
    }

    virtual nx::Buffer hash() const override
    {
        return CommandDescriptor::hash(m_transaction.params);
    }

    const Command<typename CommandDescriptor::Data>& get() const
    {
        return m_transaction;
    }

    Command<typename CommandDescriptor::Data> take()
    {
        return std::move(m_transaction);
    }

private:
    Command<typename CommandDescriptor::Data> m_transaction;
};

//-------------------------------------------------------------------------------------------------

template<typename CommandDescriptor>
std::unique_ptr<SerializableCommand<CommandDescriptor>> makeSerializer(
    Command<typename CommandDescriptor::Data> command)
{
    using Serializer = SerializableCommand<CommandDescriptor>;
    return std::make_unique<Serializer>(std::move(command));
}

} // namespace nx::clusterdb::engine
