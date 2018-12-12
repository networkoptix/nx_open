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

            // do not cache read-only transactions (they have sequence == 0)
            if (!tran.persistentInfo.isNull() && m_cache.contains(tran.persistentInfo))
                return *m_cache[tran.persistentInfo];

            QJsonValue jsonTran;
            QJson::serialize(tran, &jsonTran);

            //QJsonValue jsonParams;
            //QJson::serialize(tran.params, &jsonParams);

            QJsonObject tranObject;
            tranObject[QStringLiteral("tran")] = jsonTran;

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
            return serializedTransactionWithHeaderInternal(jsonTran, header);
        }

        QByteArray serializedTransactionWithHeaderInternal(const QJsonValue& jsonTran, const QnTransactionTransportHeader &header)
        {
            QJsonValue jsonHeader;
            QJson::serialize(header, &jsonHeader);

            QJsonObject tranObject;
            tranObject[QStringLiteral("tran")] = jsonTran;
            tranObject[QStringLiteral("header")] = jsonHeader;

            return QJson::serialized(tranObject);
        }

        /*
         * Serialize Command::Value as int to send transaction
         * to the old server with version < 4.0.
         */
        template <class T>
        QByteArray serializedLegacyTransactionWithHeader(
            const QnTransaction<T>& tran, const QnTransactionTransportHeader& header)
        {
            QJsonValue jsonTran;
            QJson::serialize(tran, &jsonTran);
            auto jsonObject = jsonTran.toObject();
            jsonObject["command"] = (int) tran.command;
            return serializedTransactionWithHeaderInternal(jsonObject, header);
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
