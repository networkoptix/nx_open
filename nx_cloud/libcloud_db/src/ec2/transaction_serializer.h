#pragma once

#include <common/common_globals.h>
#include <transaction/transaction.h>

#include "transaction_transport_header.h"

namespace nx {
namespace cdb {
namespace ec2 {

/**
 * Holds transaction inside and is able to serialize it to a requested format.
 * Can add transport header optionally.
 */
class TransactionSerializer
{
public:
    virtual ~TransactionSerializer() {}

    virtual const ::ec2::QnAbstractTransaction& transactionHeader() const = 0;
    /**
     * @note Method is re-enterable
     */
    virtual nx::Buffer serialize(
        Qn::SerializationFormat targetFormat,
        const TransactionTransportHeader& transportHeader) const = 0;
};

template<typename TransactionDataType>
class SerializedUbjsonTransaction
:
    public TransactionSerializer
{
public:
    SerializedUbjsonTransaction(
        ::ec2::QnTransaction<TransactionDataType> transaction,
        nx::Buffer serializedTransaction)
    :
        m_transaction(std::move(transaction)),
        m_serializedTransaction(serializedTransaction)
    {
    }

    virtual const ::ec2::QnAbstractTransaction& transactionHeader() const override
    {
        return m_transaction;
    }

    virtual nx::Buffer serialize(
        Qn::SerializationFormat targetFormat,
        const TransactionTransportHeader& transportHeader) const
    {
        // TODO:

        switch (targetFormat)
        {
            case Qn::UbjsonFormat:
            {
                // adding transport header
                auto serializedTransportHeader = 
                    QnUbjson::serialized(transportHeader.vmsTransportHeader);
                return serializedTransportHeader + m_serializedTransaction;
            }
            default:
                // TODO:
                NX_CRITICAL(false);
                return nx::Buffer();
        }
    }

private:
    const ::ec2::QnTransaction<TransactionDataType> m_transaction;
    const nx::Buffer m_serializedTransaction;
};

} // namespace ec2
} // namespace cdb
} // namespace nx
