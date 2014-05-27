#include "transaction_log.h"

#include <QtCore/QCryptographicHash>
#include <QtSql/QSqlQuery>
#include <QtSql/QSqlError>

#include "common/common_module.h"
#include "database/db_manager.h"
#include "transaction.h"
#include "utils/common/synctime.h"
#include "utils/common/model_functions.h"

namespace ec2
{

static QnTransactionLog* globalInstance = 0;

QnTransactionLog::QnTransactionLog(QnDbManager* db): m_dbManager(db)
{
    Q_ASSERT(!globalInstance);
    globalInstance = this;
}

void QnTransactionLog::init()
{
    QSqlQuery query(m_dbManager->getDB());
    query.prepare("SELECT peer_guid, db_guid, max(sequence) as sequence FROM transaction_log GROUP BY peer_guid, db_guid");
    if (query.exec()) {
        while (query.next()) 
        {
            QnTranStateKey key(QUuid::fromRfc4122(query.value(0).toByteArray()), QUuid::fromRfc4122(query.value(1).toByteArray()));
            m_state.insert(key, query.value(2).toInt());
        }
    }

    QSqlQuery query2(m_dbManager->getDB());
    query2.prepare("SELECT tran_guid, timestamp as timestamp FROM transaction_log");
    if (query2.exec()) {
        while (query2.next())
            m_updateHistory.insert(QUuid::fromRfc4122(query2.value(0).toByteArray()), query2.value(1).toLongLong());
    }

    m_relativeOffset = 1;
    QSqlQuery queryTime(m_dbManager->getDB());
    queryTime.prepare("SELECT max(timestamp) FROM transaction_log");
    if (queryTime.exec() && queryTime.next()) {
        m_relativeOffset = qMax(0ll, queryTime.value(0).toLongLong()) + 1;
    }

    QSqlQuery querySequence(m_dbManager->getDB());
    int startSequence = 1;
    queryTime.prepare("SELECT max(sequence) FROM transaction_log where peer_guid = ? and db_guid = ?");
    queryTime.addBindValue(qnCommon->moduleGUID().toRfc4122());
    queryTime.addBindValue(m_dbManager->getID().toRfc4122());
    if (queryTime.exec() && queryTime.next())
        startSequence = queryTime.value(0).toInt() + 1;
    QnAbstractTransaction::setStartSequence(startSequence);

    m_relativeTimer.restart();
}

qint64 QnTransactionLog::getRelativeTime() const
{
    QMutexLocker lock(&m_timeMutex);
    return m_relativeTimer.elapsed() + m_relativeOffset;
}

QnTransactionLog* QnTransactionLog::instance()
{
    return globalInstance;
}

QUuid QnTransactionLog::makeHash(const QByteArray& data1, const QByteArray& data2) const
{
    QCryptographicHash hash(QCryptographicHash::Md5);
    hash.addData(data1);
    if (!data2.isEmpty())
        hash.addData(data2);
    return QUuid::fromRfc4122(hash.result());
}

QUuid QnTransactionLog::transactionHash(const ApiResourceParamsData& params) const
{
    QCryptographicHash hash(QCryptographicHash::Md5);
    hash.addData("res_params");
    hash.addData(params.id.toRfc4122());
    foreach(const ApiResourceParamData& param, params.params) {
        hash.addData(param.name.toUtf8());
        if (param.predefinedParam)
            hash.addData("1");
    }
    return QUuid::fromRfc4122(hash.result());
}

QUuid QnTransactionLog::makeHash(const QString& extraData, const ApiCameraBookmarkTagDataList& data) {
    QCryptographicHash hash(QCryptographicHash::Md5);
    hash.addData(extraData.toUtf8());
    for(const ApiCameraBookmarkTagData tag: data)
        hash.addData(tag.name.toUtf8());
    return QUuid::fromRfc4122(hash.result());
}

ErrorCode QnTransactionLog::saveToDB(const QnAbstractTransaction& tran, const QUuid& hash, const QByteArray& data)
{
    Q_ASSERT_X(!tran.id.peerID.isNull(), Q_FUNC_INFO, "Transaction ID MUST be filled!");
    Q_ASSERT_X(!tran.id.dbID.isNull(), Q_FUNC_INFO, "Transaction ID MUST be filled!");
    Q_ASSERT_X(tran.id.sequence, Q_FUNC_INFO, "Transaction sequence MUST be filled!");
    if (tran.id.peerID == qnCommon->moduleGUID() && tran.id.dbID == m_dbManager->instance()->getID())
        Q_ASSERT(tran.timestamp > 0);

    QSqlQuery query(m_dbManager->getDB());
    //query.prepare("INSERT OR REPLACE INTO transaction_log (peer_guid, db_guid, sequence, timestamp, tran_guid, tran_data) values (?, ?, ?, ?, ?)");
    query.prepare("INSERT OR REPLACE INTO transaction_log values (?, ?, ?, ?, ?, ?)");
    query.addBindValue(tran.id.peerID.toRfc4122());
    query.addBindValue(tran.id.dbID.toRfc4122());
    query.addBindValue(tran.id.sequence);
    query.addBindValue(tran.timestamp);
    query.addBindValue(hash.toRfc4122());
    query.addBindValue(data);
    if (!query.exec()) {
        qWarning() << Q_FUNC_INFO << query.lastError().text();
        return ErrorCode::failure;
    }

    qDebug() << "add record to transaction log. Transaction=" << toString(tran.command) << "timestamp=" << tran.timestamp << "producedOnCurrentPeer=" << (tran.id.peerID == qnCommon->moduleGUID());

    QnTranStateKey key(tran.id.peerID, tran.id.dbID);
    m_state[key] = qMax(m_state[key], tran.id.sequence);
    m_updateHistory[hash] = qMax(m_updateHistory[hash], tran.timestamp);

    return ErrorCode::ok;
}

QnTranState QnTransactionLog::getTransactionsState()
{
    QReadLocker lock(&m_dbManager->getMutex());
    return m_state;
}

bool QnTransactionLog::contains(const QnAbstractTransaction& tran, const QUuid& hash) const
{
    QReadLocker lock(&m_dbManager->getMutex());
    QnTranStateKey key (tran.id.peerID, tran.id.dbID);
    Q_ASSERT(tran.id.sequence != 0);
    if (m_state.value(key) >= tran.id.sequence) {
        qDebug() << "Transaction log contains transaction " << ApiCommand::toString(tran.command) << "because of precessed seq:" << m_state.value(key) << ">=" << tran.id.sequence;
        return true;
    }
    QMap<QUuid, qint64>::const_iterator itr = m_updateHistory.find(hash);
    if (itr == m_updateHistory.end())
        return false;

    const qint64 lastTime = itr.value();
    bool rez = lastTime > tran.timestamp;
    if (lastTime == tran.timestamp) {
        QnTranStateKey locakKey(qnCommon->moduleGUID(), m_dbManager->getID());
        rez = key > locakKey;
    }
    if (rez)
        qDebug() << "Transaction log contains transaction " << ApiCommand::toString(tran.command) << "because of timestamp:" << lastTime << ">=" << tran.timestamp;

   return rez;
}

ErrorCode QnTransactionLog::getTransactionsAfter(const QnTranState& state, QList<QByteArray>& result)
{
    QReadLocker lock(&m_dbManager->getMutex());

    foreach(const QnTranStateKey& key, m_state.keys())
    {
        QSqlQuery query(m_dbManager->getDB());
        query.prepare("SELECT tran_data FROM transaction_log WHERE peer_guid = ? and db_guid = ? and sequence > ?  order by timestamp, peer_guid, db_guid, sequence");
        query.addBindValue(key.peerID.toRfc4122());
        query.addBindValue(key.dbID.toRfc4122());
        query.addBindValue(state.value(key));
        if (!query.exec())
            return ErrorCode::failure;
        
        while (query.next())
            result << query.value(0).toByteArray();
    }
    
    return ErrorCode::ok;
}

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(QnTranStateKey,    (binary),   (peerID)(dbID))

} // namespace ec2
