#pragma once

#include "../command.h"
#include "transaction_serializer.h"

namespace nx {
namespace data_sync_engine {

class NX_DATA_SYNC_ENGINE_API SerializableAbstractTransaction:
    public TransactionSerializer
{
public:
    virtual const CommandHeader& header() const = 0;
};

//-------------------------------------------------------------------------------------------------

class NX_DATA_SYNC_ENGINE_API EditableSerializableTransaction:
    public SerializableAbstractTransaction
{
public:
    virtual std::unique_ptr<EditableSerializableTransaction> clone() const = 0;
    virtual void setHeader(CommandHeader header) = 0;
};

//-------------------------------------------------------------------------------------------------

template<typename TransactionDataType>
class SerializableTransaction:
    public EditableSerializableTransaction
{
public:
    SerializableTransaction(Command<TransactionDataType> transaction):
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
        const TransactionTransportHeader& transportHeader,
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

    virtual std::unique_ptr<EditableSerializableTransaction> clone() const override
    {
        return std::make_unique<SerializableTransaction<TransactionDataType>>(
            m_transaction);
    }

    virtual void setHeader(CommandHeader header) override
    {
        m_transaction.setHeader(std::move(header));
    }

    const Command<TransactionDataType>& get() const
    {
        return m_transaction;
    }

    Command<TransactionDataType> take()
    {
        return std::move(m_transaction);
    }

private:
    Command<TransactionDataType> m_transaction;
};

} // namespace data_sync_engine
} // namespace nx
