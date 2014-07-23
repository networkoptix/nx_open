#ifndef __UBJSON_TRANSACTION_SERIALIZER_H_
#define __UBJSON_TRANSACTION_SERIALIZER_H_

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
    class QnUbjsonTransactionSerializer: public Singleton<QnUbjsonTransactionSerializer>
    {
    public:
        QnUbjsonTransactionSerializer() {}

        template<class T>
        QByteArray serializedTransaction(const QnTransaction<T>& tran) {
            QMutexLocker lock(&m_mutex);
            Q_UNUSED(lock);

            // do not cache read-only transactions (they have sequence == 0)
            if (!tran.persistentInfo.isNull() && m_cache.contains(tran.persistentInfo))
                return *m_cache[tran.persistentInfo];

            QByteArray* result = new QByteArray();
            QnUbjsonWriter<QByteArray> stream(result);
            QnUbjson::serialize( tran, &stream );
            if (!tran.persistentInfo.isNull())
                m_cache.insert(tran.persistentInfo, result);

            return *result;
        }

        template<class T>
        QByteArray serializedTransactionWithHeader(const QnTransaction<T> &tran, const QnTransactionTransportHeader &header) {
            QByteArray result;
            QnUbjsonWriter<QByteArray> stream(&result);
            QnUbjson::serialize(header, &stream);

            QByteArray serializedTran = serializedTransaction(tran);
            result.append(serializedTran);
            return result;
        }

        static bool deserializeTran(const quint8* chunkPayload, int len,  QnTransactionTransportHeader& transportHeader, QByteArray& tranData);
    private:
        mutable QMutex m_mutex;
        QCache<QnAbstractTransaction::PersistentInfo, QByteArray> m_cache;
    };

}


#endif // __UBJSON_TRANSACTION_SERIALIZER_H_
