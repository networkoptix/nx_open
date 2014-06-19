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
            if (tran.id.sequence > 0 && m_cache.contains(tran.id))
                return *m_cache[tran.id];

            QJsonValue jsonTran;
            QJson::serialize(tran, &jsonTran);

            QJsonValue jsonParams;
            QJson::serialize(tran.params, &jsonParams);

            QJsonObject tranObject;
            tranObject["tran"] = jsonTran;
            tranObject["params"] = jsonParams;
            
            QByteArray* result = new QByteArray();
            QJson::serialize(tranObject, result);

            m_cache.insert(tran.id, result);

            return *result;
        }

        template<class T>
        QByteArray serializedTransactionWithHeader(const QnTransaction<T> &tran, const QnTransactionTransportHeader &header) {
            Q_UNUSED(header);    //header is really unused in json clients
            return serializedTransaction(tran);
        }

    private:
        mutable QMutex m_mutex;
        QCache<QnAbstractTransaction::ID, QByteArray> m_cache;
    };

}

#endif // __JSON_TRANSACTION_SERIALIZER_H_
