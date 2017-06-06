#pragma once

#include <QtCore/QCache>

#include <transaction/transaction.h>
#include <transaction/transaction_transport_header.h>

#include <nx/utils/singleton.h>
#include <nx/fusion/model_functions.h>

namespace ec2
{
    class QnAbstractTransaction;

    /**
     * This class serialized a transaction into a byte array.
     */
    class QnJsonTransactionSerializer
    {
    public:
        QnJsonTransactionSerializer() {}

        template<class T>
        QByteArray serializedTransaction(const QnTransaction<T>& tran)
        {
            QnMutexLocker lock( &m_mutex );
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
        QByteArray serializedTransactionWithHeader(const QnTransaction<T> &tran, const QnTransactionTransportHeader &header)
        {
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

        QByteArray serializedTransactionWithHeader(const QByteArray &serializedTran, const QnTransactionTransportHeader &header);

        template<class T>
        QByteArray serializedTransactionWithoutHeader(const QnTransaction<T> &tran)
        {
            return serializedTransaction(tran);
        }

        QByteArray serializedTransactionWithoutHeader(const QByteArray &serializedTran);

        static bool deserializeTran(
            const quint8* chunkPayload,
            int len,
            QnTransactionTransportHeader& transportHeader,
            QByteArray& tranData );

    private:
        mutable QnMutex m_mutex;
        QCache<QnAbstractTransaction::PersistentInfo, QByteArray> m_cache;
    };
}

