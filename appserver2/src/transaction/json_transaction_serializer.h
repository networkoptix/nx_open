#ifndef __JSON_TRANSACTION_SERIALIZER_H_
#define __JSON_TRANSACTION_SERIALIZER_H_

#include <QtCore/QCache>

#include <transaction/transaction.h>
#include <transaction/transaction_transport_header.h>

#include <utils/common/singleton.h>
#include <utils/common/model_functions.h>

namespace ec2
{
    class QnAbstractTransaction;

    /**
     * This class serialized a transaction into a byte array.
     */
    class QnJsonTransactionSerializer: public Singleton<QnJsonTransactionSerializer>
    {
    public:
        QnJsonTransactionSerializer() {}

        template<class T>
        QByteArray serializeTran(const QnTransaction<T>& tran, const QnTransactionTransportHeader &header) {
            QMutexLocker lock(&m_mutex);
            Q_UNUSED(lock);

            if (m_cache.contains(tran.id))
                return *m_cache[tran.id];

            QByteArray* result = new QByteArray();
            QnOutputBinaryStream<QByteArray> stream(result);
            QByteArray headerJson = QJson::serialized(header);

            QnBinary::serialize( tran, &stream );

            m_cache.insert(tran.id, result);

            return *result;
        }

    private:
        mutable QMutex m_mutex;
        QCache<QnAbstractTransaction::ID, QByteArray> m_cache;
    };

}

#endif // __JSON_TRANSACTION_SERIALIZER_H_
