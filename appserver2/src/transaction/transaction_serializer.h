#ifndef __TRANSACTION_SERIALIZER_H_
#define __TRANSACTION_SERIALIZER_H_

#include <transaction/transaction.h>

namespace ec2
{
    class QnAbstractTransaction;

    /**
     * This class serialized a transaction into a byte array.
     */
    class QnTransactionSerializer
    {
    public:
        QnTransactionSerializer();

        template<class T>
        QByteArray serializeTran(const QnTransaction<T>& tran) {

            if (m_hash.contains(tran.id))
                return m_hash.value(tran.id);

            QByteArray result;
            QnOutputBinaryStream<QByteArray> stream(&result);
            QnBinary::serialize( tran, &stream );

            m_hash[tran.id] = result;

            return result;
        }

    private:
        QHash<QnAbstractTransaction::ID, QByteArray> m_hash;

    };
}

#endif // __TRANSACTION_SERIALIZER_H_
