#pragma once

#include <transaction/transaction.h>

#include "transaction_serializer.h"

namespace nx {
namespace cdb {
namespace ec2 {

class SerializableAbstractTransaction:
    public TransactionSerializer
{
public:
    virtual const ::ec2::QnAbstractTransaction& transactionHeader() const = 0;
};

template<typename TransactionDataType>
class SerializableTransaction:
    public SerializableAbstractTransaction
{
public:
    SerializableTransaction(::ec2::QnTransaction<TransactionDataType> transaction):
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

    virtual const ::ec2::QnAbstractTransaction& transactionHeader() const override
    {
        return m_transaction;
    }

    const ::ec2::QnTransaction<TransactionDataType>& get() const
    {
        return m_transaction;
    }

    ::ec2::QnTransaction<TransactionDataType> take()
    {
        return std::move(m_transaction);
    }

private:
    ::ec2::QnTransaction<TransactionDataType> m_transaction;
};

} // namespace ec2
} // namespace cdb
} // namespace nx
