#ifndef __TRANSACTION_LOG_H_
#define __TRANSACTION_LOG_H_

#include "nx_ec/ec_api.h"
#include "transaction.h"


namespace ec2
{
    class QnTransactionLog
    {
    public:
        static QnTransactionLog* instance();
        void initStaticInstance(QnTransactionLog* value);

        template <class T>
        ErrorCode saveTransaction(QnTransaction<T> tran) {
            BinaryStream<QByteArray> stream;
            tran.serialize(stream);
            return saveToLogInternal(stream);
        }
    private:
        ErrorCode saveToLogInternal(const BinaryStream<QByteArray>& data);
    };
};

#define transactionLog QnTransactionLog::instance()

#endif // __TRANSACTION_LOG_H_
