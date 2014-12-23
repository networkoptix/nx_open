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
        struct CacheKey 
        {
            CacheKey() {}
            CacheKey(const QnAbstractTransaction::PersistentInfo& persistentInfo, const ApiCommand::Value& command): persistentInfo(persistentInfo), command(command) {}
            QnAbstractTransaction::PersistentInfo persistentInfo;
            ApiCommand::Value command;

            bool operator== (const CacheKey& other) const {
                return persistentInfo == other.persistentInfo && command == other.command;
            }
        };
    public:
        static const int MAX_CACHE_SIZE_BYTES = 512*1024;

        QnUbjsonTransactionSerializer()
        :
            m_cache(MAX_CACHE_SIZE_BYTES)
        {
        }

        void addToCache(const QnAbstractTransaction::PersistentInfo& key, ApiCommand::Value command, const QByteArray& data) {
            QMutexLocker lock(&m_mutex);
            m_cache.insert(CacheKey(key, command), new QByteArray(data), data.size());
        }

        template<class T>
        QByteArray serializedTransaction(const QnTransaction<T>& tran) {
            QMutexLocker lock(&m_mutex);
            Q_UNUSED(lock);

            // do not cache read-only transactions (they have sequence == 0)
            CacheKey key(tran.persistentInfo, tran.command);
            if (!tran.persistentInfo.isNull() && m_cache.contains(key))
                return *m_cache[key];

            std::unique_ptr<QByteArray> serializedTran( new QByteArray() );
            QnUbjsonWriter<QByteArray> stream(serializedTran.get());
            QnUbjson::serialize( tran, &stream );
            QByteArray result = *serializedTran;
            if( !tran.persistentInfo.isNull() )
            {
                m_cache.insert( key, serializedTran.get(), serializedTran->size() );
                serializedTran.release();
            }

            return result;
        }

        template<class T>
        QByteArray serializedTransactionWithHeader(const QnTransaction<T> &tran, const QnTransactionTransportHeader &header) {
            return serializedTransactionWithHeader(serializedTransaction(tran), header);
        }

        QByteArray serializedTransactionWithHeader(const QByteArray &serializedTran, const QnTransactionTransportHeader &header) {
            QByteArray result;
            QnUbjsonWriter<QByteArray> stream(&result);
            QnUbjson::serialize(header, &stream);
            result.append(serializedTran);
            return result;
        }

        static bool deserializeTran(const quint8* chunkPayload, int len,  QnTransactionTransportHeader& transportHeader, QByteArray& tranData);
    private:

        mutable QMutex m_mutex;
        QCache<CacheKey, const QByteArray> m_cache;
    };

#ifndef QN_NO_QT
    uint qHash(const QnUbjsonTransactionSerializer::CacheKey &id);
#endif

}

#endif // __UBJSON_TRANSACTION_SERIALIZER_H_
