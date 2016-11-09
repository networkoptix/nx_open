#pragma once

#include <common/common_globals.h>
#include <transaction/transaction.h>

#include "transaction_transport_header.h"

namespace nx {
namespace cdb {
namespace ec2 {

class TransactionUbjsonDataSource
{
public:
    QByteArray serializedTransaction;
    QnUbjsonReader<QByteArray> stream;

    TransactionUbjsonDataSource(QByteArray serializedTransactionVal):
        serializedTransaction(std::move(serializedTransactionVal)),
        stream(&serializedTransaction)
    {
    }

    TransactionUbjsonDataSource(const TransactionUbjsonDataSource&) = delete;
    TransactionUbjsonDataSource& operator=(const TransactionUbjsonDataSource&) = delete;
    TransactionUbjsonDataSource(TransactionUbjsonDataSource&&) = default;
    TransactionUbjsonDataSource& operator=(TransactionUbjsonDataSource&&) = default;
};

/**
 * Holds transaction inside and is able to serialize it to a requested format.
 * Can add transport header optionally.
 */
class Serializable
{
public:
    virtual ~Serializable() = default;

    /**
     * Serialize transaction into \a targetFormat.
     * @note Method is re-enterable
     */
    virtual nx::Buffer serialize(
        Qn::SerializationFormat targetFormat,
        int transactionFormatVersion) const = 0;
    /**
     * Serialize transaction into \a targetFormat while adding transport header.
     * @note Method is re-enterable
     */
    virtual nx::Buffer serialize(
        Qn::SerializationFormat targetFormat,
        const TransactionTransportHeader& transportHeader,
        int transactionFormatVersion) const = 0;
};

template<typename Base>
class BaseUbjsonTransactionPresentation:
    public Base
{
public:
    BaseUbjsonTransactionPresentation(nx::Buffer ubjsonData):
        m_ubjsonData(std::move(ubjsonData))
    {
    }

    virtual nx::Buffer serialize(
        Qn::SerializationFormat targetFormat,
        int /*transactionFormatVersion*/) const override
    {
        switch (targetFormat)
        {
            case Qn::UbjsonFormat:
                return m_ubjsonData;
            default:
                NX_CRITICAL(false);
                return nx::Buffer();
        }
    }

    virtual nx::Buffer serialize(
        Qn::SerializationFormat targetFormat,
        const TransactionTransportHeader& transportHeader,
        int /*transactionFormatVersion*/) const override
    {
        switch (targetFormat)
        {
            case Qn::UbjsonFormat:
            {
                // adding transport header
                auto serializedTransportHeader =
                    QnUbjson::serialized(transportHeader.vmsTransportHeader);
                return serializedTransportHeader + m_ubjsonData;
            }

            default:
                NX_CRITICAL(false);
                return nx::Buffer();
        }
    }

private:
    const nx::Buffer m_ubjsonData;
};

typedef BaseUbjsonTransactionPresentation<Serializable> UbjsonTransactionPresentation;

/**
 * Extends \a Serializable functionality by providing access to transaction data.
 */
class TransactionWithSerializedPresentation:
    public Serializable
{
public:
    virtual const ::ec2::QnAbstractTransaction& transactionHeader() const = 0;
};

template<typename TransactionDataType>
class TransactionWithUbjsonPresentation:
    public BaseUbjsonTransactionPresentation<TransactionWithSerializedPresentation>
{
    typedef BaseUbjsonTransactionPresentation<TransactionWithSerializedPresentation> BaseType;

public:
    TransactionWithUbjsonPresentation(
        ::ec2::QnTransaction<TransactionDataType> transaction,
        nx::Buffer serializedTransaction)
    :
        BaseType(serializedTransaction),
        m_transaction(std::move(transaction))
    {
    }

    virtual const ::ec2::QnAbstractTransaction& transactionHeader() const override
    {
        return m_transaction;
    }

private:
    const ::ec2::QnTransaction<TransactionDataType> m_transaction;
};

} // namespace ec2
} // namespace cdb
} // namespace nx
