#include "runtime_transaction_log.h"
#include "api/runtime_info_manager.h"

namespace ec2
{

static QnRuntimeTransactionLog* globalInstance = 0;


QnRuntimeTransactionLog* QnRuntimeTransactionLog::instance()
{
    return globalInstance;
}

QnRuntimeTransactionLog::QnRuntimeTransactionLog()
{
    Q_ASSERT(!globalInstance);
    globalInstance = this;
}

void QnRuntimeTransactionLog::clearOldRuntimeData(const QnTranStateKey& key)
{
    QMutexLocker lock(&m_mutex);
    Q_ASSERT(!key.dbID.isNull());
    auto itr = m_state.values.lowerBound(QnTranStateKey(key.peerID, QUuid()));
    while (itr != m_state.values.end() && itr.key().peerID == key.peerID) 
    {
        if (itr.key().dbID == key.dbID)
            ++itr;
        else
            itr = m_state.values.erase(itr);
    }
}

void QnRuntimeTransactionLog::clearRuntimeData(const QUuid& id)
{
    QMutexLocker lock(&m_mutex);
    auto itr = m_state.values.lowerBound(QnTranStateKey(id, QUuid()));
    while (itr != m_state.values.end() && itr.key().peerID == id) 
        itr = m_state.values.erase(itr);
}

void QnRuntimeTransactionLog::clearRuntimeData()
{
    QMutexLocker lock(&m_mutex);
    m_state.values.clear();
}

bool QnRuntimeTransactionLog::contains(const QnTranState& state) const
{
    QMutexLocker lock(&m_mutex);
    for (auto itr = state.values.begin(); itr != state.values.end(); ++itr)
    {
        if (itr.value() > m_state.values.value(itr.key()))
            return false;
    }
    return true;
}

bool QnRuntimeTransactionLog::contains(const QnTransaction<ApiRuntimeData>& tran) const
{
    QMutexLocker lock(&m_mutex);
    QnTranStateKey key(tran.params.peer.id, tran.params.peer.instanceId);
    return m_state.values.value(key) >= tran.params.version;
}

ErrorCode QnRuntimeTransactionLog::saveTransaction(const QnTransaction<ApiRuntimeData>& tran)
{
    QMutexLocker lock(&m_mutex);
    QnTranStateKey key(tran.params.peer.id, tran.params.peer.instanceId);
    m_state.values[key] = tran.params.version;
    return ErrorCode::ok;
}

QnTranState QnRuntimeTransactionLog::getTransactionsState()
{
    QMutexLocker lock(&m_mutex);
    return m_state;
}

ErrorCode QnRuntimeTransactionLog::getTransactionsAfter(const QnTranState& state, QList<QnTransaction<ApiRuntimeData>>& result)
{
    QMutexLocker lock(&m_mutex);
    foreach (const QnPeerRuntimeInfo &info, QnRuntimeInfoManager::instance()->items()->getItems())
    {
        QnTranStateKey key(info.data.peer.id, info.data.peer.instanceId);
        if (info.data.version > state.values.value(key)) {
            QnTransaction<ApiRuntimeData> tran(ApiCommand::runtimeInfoChanged);
            tran.params = info.data;
            result << tran;
        }
    }
    return ErrorCode::ok;
}

} // namespace ec2
