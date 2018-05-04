#pragma once

#include <nx/fusion/serialization/ubjson_reader.h>

#include <transaction/transaction.h>

namespace nx {
namespace data_sync_engine {

/**
 * This class is to encapsulate transaction versioning support when relevant.
 */
class TransactionDeserializer
{
public:
    static bool deserialize(
        QnUbjsonReader<QByteArray>* const stream,
        ::ec2::QnAbstractTransaction* const transactionHeader,
        int transactionFormatVersion);

    template<typename TransactionData>
    static bool deserialize(
        QnUbjsonReader<QByteArray>* const stream,
        TransactionData* const transactionData,
        int /*transactionFormatVersion*/)
    {
        return QnUbjson::deserialize(stream, transactionData);
    }
};

} // namespace data_sync_engine
} // namespace nx
