#include "runtime_transaction_log.h"

#include <nx/utils/log/log.h>
#include <common/common_module.h>

namespace ec2
{

QnRuntimeTransactionLog::QnRuntimeTransactionLog(
    QnCommonModule* commonModule)
    :
    QObject(nullptr),
    QnCommonModuleAware(commonModule)
{
    const auto& manager = commonModule->runtimeInfoManager();
    connect(manager, &QnRuntimeInfoManager::runtimeInfoAdded, this, &QnRuntimeTransactionLog::at_runtimeInfoChanged, Qt::DirectConnection);
    connect(manager, &QnRuntimeInfoManager::runtimeInfoChanged, this, &QnRuntimeTransactionLog::at_runtimeInfoChanged, Qt::DirectConnection);

    at_runtimeInfoChanged(commonModule->runtimeInfoManager()->localInfo());
}

QnRuntimeTransactionLog::~QnRuntimeTransactionLog()
{
    const auto& manager = commonModule()->runtimeInfoManager();
    disconnect(manager, &QnRuntimeInfoManager::runtimeInfoAdded, this, &QnRuntimeTransactionLog::at_runtimeInfoChanged);
    disconnect(manager, &QnRuntimeInfoManager::runtimeInfoChanged, this, &QnRuntimeTransactionLog::at_runtimeInfoChanged);
}

void QnRuntimeTransactionLog::at_runtimeInfoChanged(const QnPeerRuntimeInfo& runtimeInfo)
{
    QnMutexLocker lock( &m_mutex );
    ApiPersistentIdData key(runtimeInfo.data.peer.id, runtimeInfo.data.peer.instanceId);
    m_state.values[key] = runtimeInfo.data.version;
    m_data[key] = runtimeInfo.data;
    if (runtimeInfo.data.peer.id == commonModule()->moduleGUID())
        clearOldRuntimeDataUnsafe(lock, ApiPersistentIdData(commonModule()->moduleGUID(), commonModule()->runningInstanceGUID()));
}

void QnRuntimeTransactionLog::clearOldRuntimeData(const ApiPersistentIdData& key)
{
    QnMutexLocker lock( &m_mutex );
    clearOldRuntimeDataUnsafe(lock, key);
}

void QnRuntimeTransactionLog::clearOldRuntimeDataUnsafe(QnMutexLockerBase& lock, const ApiPersistentIdData& key)
{
    NX_ASSERT(!key.persistentId.isNull());
    auto itr = m_state.values.lowerBound(ApiPersistentIdData(key.id, QnUuid()));
    bool newPeerFound = false;
    bool oldPeerFound = false;
    while (itr != m_state.values.end() && itr.key().id == key.id)
    {
        if (itr.key().persistentId == key.persistentId)
        {
            newPeerFound = true;
            ++itr;
        }
        else
        {
            oldPeerFound = true;
            m_data.remove(itr.key());
            itr = m_state.values.erase(itr);
        }
    }
    if (newPeerFound && oldPeerFound) {
        QnTransaction<ApiRuntimeData> tran(
            ApiCommand::runtimeInfoChanged,
            commonModule()->moduleGUID());
        tran.params = m_data[key];
        lock.unlock();
        emit runtimeDataUpdated(tran);
    }
}

void QnRuntimeTransactionLog::clearRuntimeData(const QnUuid& id)
{
    QnMutexLocker lock( &m_mutex );
    auto itr = m_state.values.lowerBound(ApiPersistentIdData(id, QnUuid()));
    while (itr != m_state.values.end() && itr.key().id == id)  {
        m_data.remove(itr.key());
        itr = m_state.values.erase(itr);
    }
}

void QnRuntimeTransactionLog::clearRuntimeData()
{
    QnMutexLocker lock( &m_mutex );
    m_state.values.clear();
    m_data.clear();

}

bool QnRuntimeTransactionLog::contains(const QnTranState& state) const
{
    QnMutexLocker lock( &m_mutex );
    for (auto itr = state.values.begin(); itr != state.values.end(); ++itr)
    {
        if (!m_state.values.contains(itr.key()))
        {
            NX_LOG(QnLog::EC2_TRAN_LOG, lit("Runtime info for peer %1 (dbId %2) is missing...")
                .arg(itr.key().id.toString()).arg(itr.key().persistentId.toString()), cl_logDEBUG1);
            return false;
        }

        if (itr.value() > m_state.values.value(itr.key()))
        {
            NX_LOG(QnLog::EC2_TRAN_LOG, lit("Runtime info for peer %1 (dbId %2) is old (%3 vs %4) ...")
                .arg(itr.key().id.toString()).arg(itr.key().persistentId.toString())
                .arg(m_state.values.value(itr.key())).arg(itr.value()), cl_logDEBUG1);
            return false;
        }
    }
    return true;
}

bool QnRuntimeTransactionLog::contains(const QnTransaction<ApiRuntimeData>& tran) const
{
    QnMutexLocker lock( &m_mutex );
    ApiPersistentIdData key(tran.params.peer.id, tran.params.peer.instanceId);
    return m_state.values.value(key) >= tran.params.version;
}

ErrorCode QnRuntimeTransactionLog::saveTransaction(const QnTransaction<ApiRuntimeData>& tran)
{
    QnMutexLocker lock( &m_mutex );
    ApiPersistentIdData key(tran.params.peer.id, tran.params.peer.instanceId);
    m_state.values[key] = tran.params.version;
    m_data[key] = tran.params;
    return ErrorCode::ok;
}

QnTranState QnRuntimeTransactionLog::getTransactionsState()
{
    QnMutexLocker lock( &m_mutex );
    return m_state;
}

ErrorCode QnRuntimeTransactionLog::getTransactionsAfter(const QnTranState& state, QList<QnTransaction<ApiRuntimeData>>& result)
{
    QnMutexLocker lock( &m_mutex );
    for (const ApiRuntimeData &data: m_data)
    {
        ApiPersistentIdData key(data.peer.id, data.peer.instanceId);
        if (data.version > state.values.value(key)) {
            QnTransaction<ApiRuntimeData> tran(
                ApiCommand::runtimeInfoChanged,
                commonModule()->moduleGUID());
            tran.params = data;
            result << tran;
        }
    }
    return ErrorCode::ok;
}

} // namespace ec2
