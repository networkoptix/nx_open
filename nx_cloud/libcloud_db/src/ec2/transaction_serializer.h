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

} // namespace ec2
} // namespace cdb
} // namespace nx
