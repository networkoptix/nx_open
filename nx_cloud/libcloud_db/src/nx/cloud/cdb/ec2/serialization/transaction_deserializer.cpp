#include "transaction_deserializer.h"

namespace nx {
namespace cdb {
namespace ec2 {

bool TransactionDeserializer::deserialize(
    QnUbjsonReader<QByteArray>* const stream,
    ::ec2::QnAbstractTransaction* const transactionHeader,
    int /*transactionFormatVersion*/)
{
    return QnUbjson::deserialize(stream, transactionHeader);
}

} // namespace ec2
} // namespace cdb
} // namespace nx
