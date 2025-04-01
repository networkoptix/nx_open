// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <utility>

#include <QtCore/QCache>

#include <nx/fusion/model_functions_fwd.h>
#include <nx/fusion/serialization/json_functions.h>
#include <nx/p2p/p2p_fwd.h>
#include <transaction/transaction.h>

namespace nx::p2p {

QN_FUSION_DECLARE_FUNCTIONS(TransportHeader, (json))

} // namespace nx::p2p

namespace ec2
{
    struct QnAbstractTransaction;

    /**
     * This class serialized a transaction into a byte array.
     */
    class QnJsonTransactionSerializer
    {
    public:
        QnJsonTransactionSerializer() {}


        template<class T>
        QByteArray serializedTransactionWithoutHeader(const QnTransaction<T>& tran)
        {
            return serializedTransaction(tran, /*header*/ {});
        }

        template<class T>
        QByteArray serializedTransactionWithHeader(
            const QnTransaction<T>& tran, std::optional<nx::p2p::TransportHeader> header)
        {
            return serializedTransaction(tran, std::move(header));
        }

        template<class T>
        static QJsonObject json(
            const QnTransaction<T>& tran, std::optional<nx::p2p::TransportHeader> header = {})
        {
            QJsonValue jsonTran;
            QJson::serialize(tran, &jsonTran);

            QJsonObject tranObject;
            tranObject["tran"] = jsonTran;

            if (header)
            {
                QJsonValue headerJson;
                QJson::serialize(*header, &headerJson);
                tranObject["header"] = headerJson;
            }
            return tranObject;
        }

        QByteArray serialize(
            const QJsonObject& json,
            const ec2::TransactionPersistentInfo& persistentInfo,
            bool hasHeader = false)
        {
            if (persistentInfo.isNull())
                return QJson::serialized(json);

            NX_MUTEX_LOCKER lock(&m_mutex);
            CacheKey key{persistentInfo, hasHeader};
            if (m_cache.contains(key))
                return *m_cache[key];

            QByteArray* result = new QByteArray();
            QJson::serialize(json, result);
            m_cache.insert(key, result);
            return *result;
        }

    private:
        template<class T>
        QByteArray serializedTransaction(
            const QnTransaction<T>& tran, std::optional<nx::p2p::TransportHeader> header = {})
        {
            const bool hasHeader = header.has_value();
            return serialize(json(tran, std::move(header)), tran.persistentInfo, hasHeader);
        }

    private:
        mutable nx::Mutex m_mutex;
        using CacheKey = std::pair<QnAbstractTransaction::PersistentInfo, bool>;
        QCache<CacheKey, QByteArray> m_cache;
    };
}
