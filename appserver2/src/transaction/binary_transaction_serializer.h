#ifndef __BINARY_TRANSACTION_SERIALIZER_H_
#define __BINARY_TRANSACTION_SERIALIZER_H_

#include <QtCore/QCache>

#include <transaction/transaction.h>
#include <transaction/transaction_transport_header.h>

#include <utils/common/singleton.h>
#include <utils/common/model_functions.h>

namespace ec2
{
    /**
     * This class serialized a transaction into a byte array.
     */
    class QnBinaryTransactionSerializer: public Singleton<QnBinaryTransactionSerializer>
    {
    public:
        QnBinaryTransactionSerializer() {}

        template<class T>
        QByteArray serializedTransaction(const QnTransaction<T>& tran) {
            QMutexLocker lock(&m_mutex);
            Q_UNUSED(lock);

            // do not cache read-only transactions (they have sequence == 0)
            if (!tran.persistentInfo.isNull() && m_cache.contains(tran.persistentInfo))
                return *m_cache[tran.persistentInfo];

            QByteArray* result = new QByteArray();
            QnOutputBinaryStream<QByteArray> stream(result);
            QnBinary::serialize( tran, &stream );

            if (!tran.persistentInfo.isNull())
                m_cache.insert(tran.persistentInfo, result);

            return *result;
        }

        template<class T>
        QByteArray serializedTransactionWithHeader(const QnTransaction<T> &tran, const QnTransactionTransportHeader &header) {
            return serializedTransactionWithHeader(serializedTransaction(tran), header);
        }

        QByteArray serializedTransactionWithHeader(const QByteArray &serializedTran, const QnTransactionTransportHeader &header) {
            QByteArray result;
            QnOutputBinaryStream<QByteArray> stream(&result);
            QnBinary::serialize(header, &stream);

            stream.write(serializedTran.data(), serializedTran.size());
            return result;
        }

        static bool deserializeTran(const quint8* chunkPayload, int len,  QnTransactionTransportHeader& transportHeader, QByteArray& tranData);
    private:
        mutable QMutex m_mutex;
        QCache<QnAbstractTransaction::PersistentInfo, QByteArray> m_cache;
    };

}


#endif // __BINARY_TRANSACTION_SERIALIZER_H_
