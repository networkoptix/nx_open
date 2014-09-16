#ifndef __RUNTIME_TRANSACTION_LOG_H_
#define __RUNTIME_TRANSACTION_LOG_H_

#include <QtCore/QSet>
#include <QtCore/QElapsedTimer>
#include <QtCore/QMutex>

#include "transaction.h"
#include "nx_ec/ec_api.h"
#include "nx_ec/data/api_runtime_data.h"
#include "ubjson_transaction_serializer.h"
#include "nx_ec/data/api_tran_state_data.h"

namespace ec2
{
    class QnRuntimeTransactionLog
    {
    public:

        QnRuntimeTransactionLog();

        static QnRuntimeTransactionLog* instance();
        void initStaticInstance(QnRuntimeTransactionLog* value);


        void clearOldRuntimeData(const QnTranStateKey& key);
        void clearRuntimeData(const QUuid& id);
        bool contains(const QnTranState& state) const;
        bool contains(const QnTransaction<ApiRuntimeData>& tran) const;

        ErrorCode saveTransaction(const QnTransaction<ApiRuntimeData>& tran);
        QnTranState getTransactionsState();
        ErrorCode getTransactionsAfter(const QnTranState& state, QList<QnTransaction<ApiRuntimeData>>& result);
        void clearRuntimeData();
    private:
        QnTranState m_state;
        mutable QMutex m_mutex;
    };
};

#define runtimeTransactionLog QnRuntimeTransactionLog::instance()

#endif // __TRANSACTION_LOG_H_
