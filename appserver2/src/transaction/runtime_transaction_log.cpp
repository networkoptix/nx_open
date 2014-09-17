#include "runtime_transaction_log.h"

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
        emit runtimeDataUpdated(tran);
    }
}

void QnRuntimeTransactionLog::clearRuntimeData(const QUuid& id)
{
    QMutexLocker lock(&m_mutex);
    auto itr = m_state.values.lowerBound(QnTranStateKey(id, QUuid()));
    while (itr != m_state.values.end() && itr.key().peerID == id)  {
        m_data.remove(itr.key());
        itr = m_state.values.erase(itr);
    }
}

void QnRuntimeTransactionLog::clearRuntimeData()
{
    QMutexLocker lock(&m_mutex);
    m_state.values.clear();
    m_data.clear();
    
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
    m_data[key] = tran.params;
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
    foreach (const ApiRuntimeData &data, m_data)
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
