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
        QByteArray serializedTransaction(const QnTransaction<T>& tran) {
            QMutexLocker lock(&m_mutex);
            Q_UNUSED(lock);

            // do not cache read-only transactions (they have sequence == 0)
            if (!tran.persistentInfo.isNull() && m_cache.contains(tran.persistentInfo))
                return *m_cache[tran.persistentInfo];

            QJsonValue jsonTran;
            QJson::serialize(tran, &jsonTran);

            QJsonValue jsonParams;
            QJson::serialize(tran.params, &jsonParams);

            QJsonObject tranObject;
            tranObject["tran"] = jsonTran;
            
            QByteArray* result = new QByteArray();
            QJson::serialize(tranObject, result);

            if (!tran.persistentInfo.isNull())
                m_cache.insert(tran.persistentInfo, result);

            return *result;
        }

        template<class T>
        QByteArray serializedTransactionWithHeader(const QnTransaction<T> &tran, const QnTransactionTransportHeader &header) {
            Q_UNUSED(header);    //header is really unused in json clients
            return serializedTransaction(tran);
        }

        QByteArray serializedTransactionWithHeader(const QByteArray &serializedTran, const QnTransactionTransportHeader &header) {
            Q_UNUSED(header);    //header is really unused in json clients
            return serializedTran;
        }

    private:
        mutable QMutex m_mutex;
        QCache<QnAbstractTransaction::PersistentInfo, QByteArray> m_cache;
    };

}

#endif // __JSON_TRANSACTION_SERIALIZER_H_
