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

            //QJsonValue jsonParams;
            //QJson::serialize(tran.params, &jsonParams);

            QJsonObject tranObject;
            tranObject["tran"] = jsonTran;
            
            QByteArray* result = new QByteArray();
            QJson::serialize(tranObject, result);

            if (!tran.persistentInfo.isNull())
                m_cache.insert(tran.persistentInfo, result);    //TODO #ak add with/without header mark

            return *result;
        }

        template<class T>
        QByteArray serializedTransactionWithHeader(const QnTransaction<T> &tran, const QnTransactionTransportHeader &header) {
            //TODO #ak use cache
            QJsonValue jsonTran;
            QJson::serialize(tran, &jsonTran);

            QJsonValue jsonHeader;
            QJson::serialize(header, &jsonHeader);

            QJsonObject tranObject;
            tranObject["tran"] = jsonTran;
            tranObject["header"] = jsonHeader;

            return QJson::serialized(tranObject);
        }

        QByteArray serializedTransactionWithHeader(const QByteArray &serializedTran, const QnTransactionTransportHeader &header) {

            QJsonValue jsonHeader;
            QJson::serialize(header, &jsonHeader);
            const QByteArray& serializedHeader = QJson::serialized(jsonHeader);

            return 
                "{\n"
                    "\"header\": "+serializedHeader+",\n"
                    "\"tran\": "+serializedTran+"\n"
                "}";
        }

        template<class T>
        QByteArray serializedTransactionWithoutHeader(const QnTransaction<T> &tran, const QnTransactionTransportHeader &header) {
            Q_UNUSED(header);    //header is really unused in json clients
            return serializedTransaction(tran);
        }

        QByteArray serializedTransactionWithoutHeader(const QByteArray &serializedTran, const QnTransactionTransportHeader &header) {
            Q_UNUSED(header);    //header is really unused in json clients
            return serializedTran;
        }

        static bool deserializeTran(
            const quint8* chunkPayload,
            int len,
            QnTransactionTransportHeader& transportHeader,
            QByteArray& tranData )
        {
            QByteArray srcData = QByteArray::fromRawData((const char*) chunkPayload, len);
            QJsonObject tranObject;
            if( !QJson::deserialize(srcData, &tranObject) )
                return false;
            if( !QJson::deserialize( tranObject["header"], &transportHeader ) )
                return false;
            tranData = QByteArray( (const char*)chunkPayload, len );
            return true;
        }

    private:
        mutable QMutex m_mutex;
        QCache<QnAbstractTransaction::PersistentInfo, QByteArray> m_cache;
    };
}

#endif // __JSON_TRANSACTION_SERIALIZER_H_
