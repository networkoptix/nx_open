#include "transaction_log.h"

namespace ec2
{

static QnTransactionLog* globalInstance = 0;

QnTransactionLog* QnTransactionLog::instance()
{
    return globalInstance;
}

void QnTransactionLog::initStaticInstance(QnTransactionLog* value)
{
    globalInstance = value;
}

QnTransactionLog::ErrorCode QnTransactionLog::saveToLogInternal(const BinaryStream<QByteArray>& data)
{
    // todo: implement me
    return No_Error;
}

}
