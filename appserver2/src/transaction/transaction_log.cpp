#include "transaction_log.h"
#include "common/common_module.h"
#include "database/db_manager.h"
#include "transaction.h"

namespace ec2
{

static QnTransactionLog* globalInstance = 0;

QnTransactionLog::QnTransactionLog(QnDbManager* db): m_dbManager(db)
{
    Q_ASSERT(!globalInstance);
    globalInstance = this;

    QSqlQuery query(m_dbManager->getDB());
    query.prepare("SELECT distinct peer_guid FROM transaction_log");
    if (query.exec()) {
        while (query.next())
            m_peerList << QUuid::fromRfc4122(query.value(0).toByteArray());
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

ErrorCode QnTransactionLog::saveToDB(const QnAbstractTransaction::ID& tranID, const QUuid& hash, const QByteArray& data)
{
    m_peerList.insert(tranID.peerGUID);

    QSqlQuery query(m_dbManager->getDB());
    query.prepare("INSERT OR REPLACE INTO transaction_log (peer_guid, sequence, tran_guid, tran_data) values (?, ?, ?, ?)");
    query.bindValue(0, tranID.peerGUID);
    query.bindValue(1, tranID.sequence);
    query.bindValue(2, hash);
    query.bindValue(3, data);
    if (!query.exec()) {
        qWarning() << Q_FUNC_INFO << query.lastError().text();
        return ErrorCode::failure;
    }

    return ErrorCode::ok;
}

ErrorCode QnTransactionLog::getTransactionsState(QnTranState& state)
{
    QReadLocker lock(&m_dbManager->getMutex());

    state.clear();
    QSqlQuery query(m_dbManager->getDB());
    query.prepare("SELECT peer_guid, max(sequence) as sequence FROM transaction_log GROUP BY peer_guid");
    if (query.exec())
        return ErrorCode::failure;
    while (query.next())
        state.insert(QUuid::fromRfc4122(query.value(0).toByteArray()), query.value(1).toInt());
    return ErrorCode::ok;
}

ErrorCode QnTransactionLog::getTransactionsAfter(const QnTranState& state, QList<QByteArray>& result)
{
    QReadLocker lock(&m_dbManager->getMutex());

    foreach(const QUuid& peerGuid, m_peerList)
    {
        QSqlQuery query(m_dbManager->getDB());
        query.prepare("SELECT tran_data FROM transaction_log WHERE peer_guid = ? and sequence > ?");
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
