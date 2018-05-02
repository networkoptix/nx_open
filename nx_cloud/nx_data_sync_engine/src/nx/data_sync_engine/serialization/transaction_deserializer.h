#pragma once

#include <nx/fusion/serialization/ubjson_reader.h>

#include <transaction/transaction.h>

namespace nx {
namespace cdb {
namespace ec2 {

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

} // namespace ec2
} // namespace cdb
} // namespace nx
