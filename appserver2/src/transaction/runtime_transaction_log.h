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
#include "api/runtime_info_manager.h"

namespace ec2
{
    class QnRuntimeTransactionLog: public QObject
    {
        Q_OBJECT
    public:

        QnRuntimeTransactionLog(QObject* parent = NULL);
		~QnRuntimeTransactionLog();

        void clearOldRuntimeData(const QnTranStateKey& key);
        void clearRuntimeData(const QnUuid& id);
        bool contains(const QnTranState& state) const;
        bool contains(const QnTransaction<ApiRuntimeData>& tran) const;

        ErrorCode saveTransaction(const QnTransaction<ApiRuntimeData>& tran);
        QnTranState getTransactionsState();
        ErrorCode getTransactionsAfter(const QnTranState& state, QList<QnTransaction<ApiRuntimeData>>& result);
        void clearRuntimeData();
signals:
        /*
        * In case if some peer has been restarted, but old runtine data (with old instaceID) still present in the cloud.
        * So, old data will be removed soon. If we have 2 different data for same peer and old data has been deleted
        * this signal is emitted
        */
        void runtimeDataUpdated(const QnTransaction<ApiRuntimeData>& data);
private slots:
        void at_runtimeInfoChanged(const QnPeerRuntimeInfo& runtimeInfo);
    private:
        QnTranState m_state;
        QMap<QnTranStateKey, ApiRuntimeData> m_data;
        mutable QMutex m_mutex;
    };
};

#endif // __TRANSACTION_LOG_H_
