#pragma once

#include <QtCore/QSet>
#include <nx/utils/thread/mutex.h>

#include "transaction.h"
#include "nx_ec/ec_api.h"
#include "nx_ec/data/api_runtime_data.h"
#include "ubjson_transaction_serializer.h"
#include <nx/vms/api/data/tran_state_data.h>
#include "api/runtime_info_manager.h"
#include <common/common_module_aware.h>

namespace ec2 {

class QnRuntimeTransactionLog: public QObject, public QnCommonModuleAware
{
    Q_OBJECT
public:

    QnRuntimeTransactionLog(QnCommonModule* commonModule);
	    ~QnRuntimeTransactionLog();

    void clearOldRuntimeData(const nx::vms::api::PersistentIdData& key);
    void clearRuntimeData(const QnUuid& id);
    bool contains(const nx::vms::api::TranState& state) const;
    bool contains(const QnTransaction<ApiRuntimeData>& tran) const;

    ErrorCode saveTransaction(const QnTransaction<ApiRuntimeData>& tran);
    nx::vms::api::TranState getTransactionsState();
    ErrorCode getTransactionsAfter(const nx::vms::api::TranState& state,
        QList<QnTransaction<ApiRuntimeData>>& result);
    void clearRuntimeData();

signals:
    /*
    * In case if some peer has been restarted, but old runtine data (with old instaceID) still present in the cloud.
    * So, old data will be removed soon. If we have 2 different data for same peer and old data has been deleted
    * this signal is emitted
    */
    void runtimeDataUpdated(const QnTransaction<ApiRuntimeData>& data);

private:
    void clearOldRuntimeDataUnsafe(
        QnMutexLockerBase& lock, const nx::vms::api::PersistentIdData& key);

private slots:
    void at_runtimeInfoChanged(const QnPeerRuntimeInfo& runtimeInfo);

private:
    nx::vms::api::TranState m_state;
    QMap<nx::vms::api::PersistentIdData, ApiRuntimeData> m_data;
    mutable QnMutex m_mutex;
};

} // namespace ec2
