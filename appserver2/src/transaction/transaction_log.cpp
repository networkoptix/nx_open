#include "transaction_log.h"
#include "common/common_module.h"
#include "database/db_manager.h"
#include "transaction.h"
#include "utils/common/synctime.h"

namespace ec2
{

static QnTransactionLog* globalInstance = 0;

QnTransactionLog::QnTransactionLog(QnDbManager* db): m_dbManager(db)
{
    Q_ASSERT(!globalInstance);
    globalInstance = this;

    QSqlQuery query(m_dbManager->getDB());
    query.prepare("SELECT peer_guid, max(sequence) as sequence FROM transaction_log GROUP BY peer_guid");
    if (query.exec()) {
        while (query.next())
            m_state.insert(QUuid::fromRfc4122(query.value(0).toByteArray()), query.value(1).toInt());
    }

    QSqlQuery query2(m_dbManager->getDB());
    query2.prepare("SELECT tran_guid, timestamp as timestamp FROM transaction_log");
    if (query2.exec()) {
        while (query2.next())
            m_updateHistory.insert(QUuid::fromRfc4122(query.value(0).toByteArray()), query.value(1).toInt());
    }

}

QnTransactionLog* QnTransactionLog::instance()
{
    return globalInstance;
}

QUuid QnTransactionLog::makeHash(const QByteArray& data1, const QByteArray& data2)
{
    QCryptographicHash hash(QCryptographicHash::Md5);
    hash.addData(data1);
    if (!data2.isEmpty())
        hash.addData(data2);
    return QUuid::fromRfc4122(hash.result());
}

ErrorCode QnTransactionLog::saveToDB(const QnAbstractTransaction& tran, const QUuid& hash, const QByteArray& data)
{
    Q_ASSERT_X(!tran.id.peerGUID.isNull(), Q_FUNC_INFO, "Transaction ID MUST be filled!");

    QSqlQuery query(m_dbManager->getDB());
    query.prepare("INSERT OR REPLACE INTO transaction_log (peer_guid, sequence, timestamp, tran_guid, tran_data) values (?, ?, ?, ?, ?)");
    query.bindValue(0, tran.id.peerGUID.toRfc4122());
    query.bindValue(1, tran.id.sequence);
    query.bindValue(2, tran.timestamp/1000);
    query.bindValue(3, hash);
    query.bindValue(4, data);
    if (!query.exec()) {
        qWarning() << Q_FUNC_INFO << query.lastError().text();
        return ErrorCode::failure;
    }

    m_state[tran.id.peerGUID] = qMax(m_state[tran.id.peerGUID], tran.id.sequence);
    m_updateHistory[hash] = qMax(m_updateHistory[hash], tran.timestamp);

    return ErrorCode::ok;
}

QnTranState QnTransactionLog::getTransactionsState()
{
    QReadLocker lock(&m_dbManager->getMutex());
    return m_state;
}

bool QnTransactionLog::contains(const QnAbstractTransaction& tran, const QUuid& hash)
{
    QReadLocker lock(&m_dbManager->getMutex());
    return m_state.value(tran.id.peerGUID) >= tran.id.sequence ||
           m_updateHistory.value(hash) > tran.timestamp;
}

ErrorCode QnTransactionLog::getTransactionsAfter(const QnTranState& state, QList<QByteArray>& result)
{
    QReadLocker lock(&m_dbManager->getMutex());

    foreach(const QUuid& peerGuid, m_state.keys())
    {
        QSqlQuery query(m_dbManager->getDB());
        query.prepare("SELECT tran_data FROM transaction_log WHERE peer_guid = ? and sequence > ?  order by timestamp");
        query.bindValue(0, peerGuid.toRfc4122());
        query.bindValue(1, state.value(peerGuid));
        if (!query.exec())
            return ErrorCode::failure;
        
        while (query.next())
            result << query.value(0).toByteArray();
    }
    
    return ErrorCode::ok;
}

}
