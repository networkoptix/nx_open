#include "transaction_deserializer.h"

namespace nx {
namespace data_sync_engine {

bool TransactionDeserializer::deserialize(
    QnUbjsonReader<QByteArray>* const stream,
    ::ec2::QnAbstractTransaction* const transactionHeader,
    int /*transactionFormatVersion*/)
{
    return QnUbjson::deserialize(stream, transactionHeader);
}

} // namespace data_sync_engine
} // namespace nx
