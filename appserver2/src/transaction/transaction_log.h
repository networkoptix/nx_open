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
            QByteArray serializedTran;
            OutputBinaryStream<QByteArray> stream( &serializedTran );
            tran.serialize(&stream);
            return saveToLogInternal(serializedTran);
        }
    private:
        ErrorCode saveToLogInternal(const QByteArray& data);
    };
};

#define transactionLog QnTransactionLog::instance()

#endif // __TRANSACTION_LOG_H_
