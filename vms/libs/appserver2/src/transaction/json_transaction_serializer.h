// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QCache>

#include <nx/fusion/serialization/json_functions.h>
#include <nx/utils/singleton.h>
#include <transaction/transaction.h>

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
        QByteArray serializedTransaction(const QnTransaction<T>& tran)
        {
            NX_MUTEX_LOCKER lock( &m_mutex );

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
                m_cache.insert(tran.persistentInfo, result);    //TODO #akolesnikov add with/without header mark

            return *result;
        }

        template<class T>
        QByteArray serializedTransactionWithoutHeader(const QnTransaction<T> &tran)
        {
            return serializedTransaction(tran);
        }

    private:
        mutable nx::Mutex m_mutex;
        QCache<QnAbstractTransaction::PersistentInfo, QByteArray> m_cache;
    };
}
