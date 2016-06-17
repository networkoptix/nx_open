#include "runtime_transaction_log.h"

#include <utils/common/log.h>


namespace ec2
{

QnRuntimeTransactionLog::QnRuntimeTransactionLog(QObject* parent):
    QObject(parent)
{ 
    connect(QnRuntimeInfoManager::instance(), &QnRuntimeInfoManager::runtimeInfoAdded, this, &QnRuntimeTransactionLog::at_runtimeInfoChanged, Qt::DirectConnection);
    connect(QnRuntimeInfoManager::instance(), &QnRuntimeInfoManager::runtimeInfoChanged, this, &QnRuntimeTransactionLog::at_runtimeInfoChanged, Qt::DirectConnection);
}

QnRuntimeTransactionLog::~QnRuntimeTransactionLog()
{
    disconnect(QnRuntimeInfoManager::instance(), &QnRuntimeInfoManager::runtimeInfoAdded, this, &QnRuntimeTransactionLog::at_runtimeInfoChanged);
    disconnect(QnRuntimeInfoManager::instance(), &QnRuntimeInfoManager::runtimeInfoChanged, this, &QnRuntimeTransactionLog::at_runtimeInfoChanged);
}

void QnRuntimeTransactionLog::at_runtimeInfoChanged(const QnPeerRuntimeInfo& runtimeInfo)
{
    QnMutexLocker lock( &m_mutex );
    QnTranStateKey key(runtimeInfo.data.peer.id, runtimeInfo.data.peer.instanceId);
    m_state.values[key] = runtimeInfo.data.version;
    m_data[key] = runtimeInfo.data;
    if (runtimeInfo.data.peer.id == qnCommon->moduleGUID())
        clearOldRuntimeDataUnsafe(lock, QnTranStateKey(qnCommon->moduleGUID(), qnCommon->runningInstanceGUID()));
}

void QnRuntimeTransactionLog::clearOldRuntimeData(const QnTranStateKey& key)
{
    QnMutexLocker lock( &m_mutex );
    clearOldRuntimeDataUnsafe(lock, key);
}

void QnRuntimeTransactionLog::clearOldRuntimeDataUnsafe(QnMutexLockerBase& lock, const QnTranStateKey& key)
{
    NX_ASSERT(!key.dbID.isNull());
    auto itr = m_state.values.lowerBound(QnTranStateKey(key.peerID, QnUuid()));
    bool newPeerFound = false;
    bool oldPeerFound = false;
    while (itr != m_state.values.end() && itr.key().peerID == key.peerID) 
    {
        if (itr.key().dbID == key.dbID) {
            newPeerFound = true;
            ++itr;
        }
        else {
            oldPeerFound = true;
            m_data.remove(itr.key());
            itr = m_state.values.erase(itr);
        }
    }
    if (newPeerFound && oldPeerFound) {
        QnTransaction<ApiRuntimeData> tran(ApiCommand::runtimeInfoChanged);
        tran.params = m_data[key];
        lock.unlock();
        emit runtimeDataUpdated(tran);
    }
}

void QnRuntimeTransactionLog::clearRuntimeData(const QnUuid& id)
{
    QnMutexLocker lock( &m_mutex );
    auto itr = m_state.values.lowerBound(QnTranStateKey(id, QnUuid()));
    while (itr != m_state.values.end() && itr.key().peerID == id)  {
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
                .arg(itr.key().peerID.toString()).arg(itr.key().dbID.toString()), cl_logDEBUG1);
            return false;
        }

        if (itr.value() > m_state.values.value(itr.key()))
        {
            NX_LOG(QnLog::EC2_TRAN_LOG, lit("Runtime info for peer %1 (dbId %2) is old (%3 vs %4) ...")
                .arg(itr.key().peerID.toString()).arg(itr.key().dbID.toString())
                .arg(m_state.values.value(itr.key())).arg(itr.value()), cl_logDEBUG1);
            return false;
        }
    }
    return true;
}

bool QnRuntimeTransactionLog::contains(const QnTransaction<ApiRuntimeData>& tran) const
{
    QnMutexLocker lock( &m_mutex );
    QnTranStateKey key(tran.params.peer.id, tran.params.peer.instanceId);
    return m_state.values.value(key) >= tran.params.version;
}

ErrorCode QnRuntimeTransactionLog::saveTransaction(const QnTransaction<ApiRuntimeData>& tran)
{
    QnMutexLocker lock( &m_mutex );
    QnTranStateKey key(tran.params.peer.id, tran.params.peer.instanceId);
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
        QnTranStateKey key(data.peer.id, data.peer.instanceId);
        if (data.version > state.values.value(key)) {
            QnTransaction<ApiRuntimeData> tran(ApiCommand::runtimeInfoChanged);
            tran.params = data;
            result << tran;
        }
    }
    return ErrorCode::ok;
}

} // namespace ec2
