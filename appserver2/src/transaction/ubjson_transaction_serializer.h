#ifndef __UBJSON_TRANSACTION_SERIALIZER_H_
#define __UBJSON_TRANSACTION_SERIALIZER_H_

#include <memory>

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
        static const int MAX_CACHE_SIZE_BYTES = 1*1024*1024;

        QnUbjsonTransactionSerializer()
        :
            m_cache(MAX_CACHE_SIZE_BYTES)
        {
        }

        template<class T>
        QByteArray serializedTransaction(const QnTransaction<T>& tran) {
            QMutexLocker lock(&m_mutex);
            Q_UNUSED(lock);

            // do not cache read-only transactions (they have sequence == 0)
            if (!tran.persistentInfo.isNull() && m_cache.contains(tran.persistentInfo))
                return *m_cache[tran.persistentInfo];

            std::unique_ptr<QByteArray> serializedTran( new QByteArray() );
            QnUbjsonWriter<QByteArray> stream(serializedTran.get());
            QnUbjson::serialize( tran, &stream );
            QByteArray result = *serializedTran;
            if (!tran.persistentInfo.isNull())
                if( m_cache.insert(tran.persistentInfo, serializedTran.get(), serializedTran->size()) )
                    serializedTran.release();

            return result;
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
