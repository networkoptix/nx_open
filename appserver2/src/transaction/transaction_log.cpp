#include "transaction_log.h"

#include <QtCore/QCryptographicHash>
#include <QtSql/QSqlQuery>
#include <QtSql/QSqlError>

#include "common/common_module.h"
#include "database/db_manager.h"
#include "transaction.h"
#include "utils/common/synctime.h"
#include "utils/common/model_functions.h"
#include "nx_ec/data/api_discovery_data.h"
#include "nx_ec/data/api_camera_bookmark_data.h"

#define TRANSACTION_MESSAGE_BUS_DEBUG

namespace ec2
{

static QnTransactionLog* globalInstance = 0;

QnTransactionLog::QnTransactionLog(QnDbManager* db): m_dbManager(db)
{
    Q_ASSERT(!globalInstance);
    globalInstance = this;
    m_lastTimestamp = 0;
    m_currentTime = 0;
}

void QnTransactionLog::init()
{
    QSqlQuery query(m_dbManager->getDB());
    query.prepare("SELECT peer_guid, db_guid, max(sequence) as sequence FROM transaction_log GROUP BY peer_guid, db_guid");
    if (query.exec()) {
        while (query.next()) 
        {
            QnTranStateKey key(QUuid::fromRfc4122(query.value(0).toByteArray()), QUuid::fromRfc4122(query.value(1).toByteArray()));
            m_state.values.insert(key, query.value(2).toInt());
        }
    }

    QSqlQuery query2(m_dbManager->getDB());
    query2.prepare("SELECT tran_guid, timestamp, peer_guid, db_guid FROM transaction_log"); 
    if (query2.exec()) {
        while (query2.next()) {
            QUuid hash = QUuid::fromRfc4122(query2.value("tran_guid").toByteArray());
            qint64 timestamp = query2.value("timestamp").toLongLong();
            QUuid peerID = QUuid::fromRfc4122(query2.value("peer_guid").toByteArray());
            QUuid dbID = QUuid::fromRfc4122(query2.value("db_guid").toByteArray());
            m_updateHistory.insert(hash, UpdateHistoryData(QnTranStateKey(peerID, dbID), timestamp));
        }     
    }

    m_currentTime = QDateTime::currentMSecsSinceEpoch();
    QSqlQuery queryTime(m_dbManager->getDB());
    queryTime.prepare("SELECT max(timestamp) FROM transaction_log");
    if (queryTime.exec() && queryTime.next()) {
        m_currentTime = qMax(m_currentTime, queryTime.value(0).toLongLong());
    }
    m_relativeTimer.start();

    QSqlQuery querySequence(m_dbManager->getDB());
    int startSequence = 1;
    queryTime.prepare("SELECT max(sequence) FROM transaction_log where peer_guid = ? and db_guid = ?");
    queryTime.addBindValue(qnCommon->moduleGUID().toRfc4122());
    queryTime.addBindValue(m_dbManager->getID().toRfc4122());
    if (queryTime.exec() && queryTime.next())
        startSequence = queryTime.value(0).toInt() + 1;
    QnAbstractTransaction::setStartSequence(startSequence);

}

qint64 QnTransactionLog::getTimeStamp()
{
    QMutexLocker lock(&m_timeMutex);
    qint64 timestamp = m_currentTime + m_relativeTimer.elapsed();
    if (timestamp <= m_lastTimestamp) {
        m_currentTime = timestamp = m_lastTimestamp + 1;
        m_relativeTimer.restart();
    }
    m_lastTimestamp = timestamp;
    return timestamp;
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

QUuid QnTransactionLog::makeHash(const QString& extraData, const ApiCameraBookmarkTagDataList& data) const {
    QCryptographicHash hash(QCryptographicHash::Md5);
    hash.addData(extraData.toUtf8());
    for(const ApiCameraBookmarkTagData tag: data)
        hash.addData(tag.name.toUtf8());
    return QUuid::fromRfc4122(hash.result());
}

QUuid QnTransactionLog::makeHash(const QString &extraData, const ApiDiscoveryDataList &data) const {
    QCryptographicHash hash(QCryptographicHash::Md5);
    hash.addData(extraData.toUtf8());
    foreach (const ApiDiscoveryData &item, data) {
        hash.addData(item.url.toUtf8());
        hash.addData(item.id.toString().toUtf8());
    }
    return QUuid::fromRfc4122(hash.result());
}

ErrorCode QnTransactionLog::updateSequence(const QnAbstractTransaction& tran)
{
    QSqlQuery query(m_dbManager->getDB());
    //query.prepare("INSERT OR REPLACE INTO transaction_sequence (peer_guid, db_guid, sequence) values (?, ?, ?)");
    query.prepare("INSERT OR REPLACE INTO transaction_sequence values (?, ?, ?)");
    query.addBindValue(tran.peerID.toRfc4122());
    query.addBindValue(tran.persistentInfo.dbID.toRfc4122());
    query.addBindValue(tran.persistentInfo.sequence);
    if (!query.exec()) {
        qWarning() << Q_FUNC_INFO << query.lastError().text();
        return ErrorCode::failure;
    }
    return ErrorCode::ok;
}

ErrorCode QnTransactionLog::saveToDB(const QnAbstractTransaction& tran, const QUuid& hash, const QByteArray& data)
{
    if (tran.command != ApiCommand::updatePersistentSequence)
    {
    #ifdef TRANSACTION_MESSAGE_BUS_DEBUG
        qDebug() << "add transaction to log " << tran.peerID << "command=" << ApiCommand::toString(tran.command) 
            << "db seq=" << tran.persistentInfo.sequence << "timestamp=" << tran.persistentInfo.timestamp;
    #endif

        Q_ASSERT_X(!tran.peerID.isNull(), Q_FUNC_INFO, "Transaction ID MUST be filled!");
        Q_ASSERT_X(!tran.persistentInfo.dbID.isNull(), Q_FUNC_INFO, "Transaction ID MUST be filled!");
        Q_ASSERT_X(tran.persistentInfo.sequence, Q_FUNC_INFO, "Transaction sequence MUST be filled!");
        if (tran.peerID == qnCommon->moduleGUID() && tran.persistentInfo.dbID == m_dbManager->instance()->getID())
            Q_ASSERT(tran.persistentInfo.timestamp > 0);

        QSqlQuery query(m_dbManager->getDB());
        //query.prepare("INSERT OR REPLACE INTO transaction_log (peer_guid, db_guid, sequence, timestamp, tran_guid, tran_data) values (?, ?, ?, ?, ?)");
        query.prepare("INSERT OR REPLACE INTO transaction_log values (?, ?, ?, ?, ?, ?)");
        query.addBindValue(tran.peerID.toRfc4122());
        query.addBindValue(tran.persistentInfo.dbID.toRfc4122());
        query.addBindValue(tran.persistentInfo.sequence);
        query.addBindValue(tran.persistentInfo.timestamp);
        query.addBindValue(hash.toRfc4122());
        query.addBindValue(data);
        if (!query.exec()) {
            qWarning() << Q_FUNC_INFO << query.lastError().text();
            return ErrorCode::failure;
        }
    }

    ErrorCode code = updateSequence(tran);
    if (code != ErrorCode::ok)
        return code;

#ifdef TRANSACTION_LOG_DEBUG
    qDebug() << "add record to transaction log. Transaction=" << toString(tran.command) << "timestamp=" << tran.timestamp << "producedOnCurrentPeer=" << (tran.peerID == qnCommon->moduleGUID());
#endif

    QnTranStateKey key(tran.peerID, tran.persistentInfo.dbID);
    m_state.values[key] = qMax(m_state.values[key], tran.persistentInfo.sequence);

    auto updateHistoryItr = m_updateHistory.find(hash);
    if (updateHistoryItr == m_updateHistory.end())
        updateHistoryItr = m_updateHistory.insert(hash, UpdateHistoryData());
    if ((tran.persistentInfo.timestamp > updateHistoryItr.value().timestamp) ||
        (tran.persistentInfo.timestamp == updateHistoryItr.value().timestamp && key > updateHistoryItr.value().updatedBy))
    {
        updateHistoryItr.value().timestamp = tran.persistentInfo.timestamp;
        updateHistoryItr.value().updatedBy = key;
    }

    QMutexLocker lock(&m_timeMutex);
    if (tran.persistentInfo.timestamp > m_currentTime + m_relativeTimer.elapsed()) {
        m_currentTime = m_lastTimestamp = tran.persistentInfo.timestamp;
        m_relativeTimer.restart();
    }

    return ErrorCode::ok;
}

QnTranState QnTransactionLog::getTransactionsState()
{
    QReadLocker lock(&m_dbManager->getMutex());
    return m_state;
}

QnTransactionLog::ContainsReason QnTransactionLog::contains(const QnAbstractTransaction& tran, const QUuid& hash) const
{

    QReadLocker lock(&m_dbManager->getMutex());
    QnTranStateKey key (tran.peerID, tran.persistentInfo.dbID);
    Q_ASSERT(tran.persistentInfo.sequence != 0);
    if (m_state.values.value(key) >= tran.persistentInfo.sequence) {
        qDebug() << "Transaction log contains transaction " << ApiCommand::toString(tran.command) << "because of precessed seq:" << m_state.values.value(key) << ">=" << tran.persistentInfo.sequence;
        return Reason_Sequence;
    }
    auto itr = m_updateHistory.find(hash);
    if (itr == m_updateHistory.end())
        return Reason_None;

    const qint64 lastTime = itr.value().timestamp;
    bool rez = lastTime > tran.persistentInfo.timestamp;
    if (lastTime == tran.persistentInfo.timestamp)
        rez = key < itr.value().updatedBy;
    if (rez) {
        qDebug() << "Transaction log contains transaction " << ApiCommand::toString(tran.command) << "because of timestamp:" << lastTime << ">=" << tran.persistentInfo.timestamp;
        return Reason_Timestamp;
    }
    else {
        return Reason_None;
    }
}

ErrorCode QnTransactionLog::getTransactionsAfter(const QnTranState& state, QList<QByteArray>& result)
{
    QReadLocker lock(&m_dbManager->getMutex());
    QMap <QnTranStateKey, int> tranLogSequence;
    foreach(const QnTranStateKey& key, m_state.values.keys())
    {
        QSqlQuery query(m_dbManager->getDB());
        query.prepare("SELECT tran_data, sequence FROM transaction_log WHERE peer_guid = ? and db_guid = ? and sequence > ?  order by timestamp, peer_guid, db_guid, sequence");
        query.addBindValue(key.peerID.toRfc4122());
        query.addBindValue(key.dbID.toRfc4122());
        query.addBindValue(state.values.value(key));
        if (!query.exec())
            return ErrorCode::failure;
        
        while (query.next()) {
            result << query.value(0).toByteArray();
            auto itrLastSeq = tranLogSequence.find(key);
            int seq = query.value(1).toInt();
            if (itrLastSeq == tranLogSequence.end())
                itrLastSeq = tranLogSequence.insert(key, seq);
            else
                itrLastSeq.value() = qMax(itrLastSeq.value(), seq);
        }
    }

    // join fillter transactions to update state to the latest available sequences
    QSqlQuery query(m_dbManager->getDB());
    query.prepare("SELECT peer_guid, db_guid, sequence from transaction_sequence");
    if (!query.exec())
        return ErrorCode::failure;
    
    while (query.next()) 
    {
        QnTranStateKey key(QUuid::fromRfc4122(query.value(0).toByteArray()), QUuid::fromRfc4122(query.value(1).toByteArray()));
        int latestSequence =  query.value(2).toInt();
        if (latestSequence > tranLogSequence.value(key)) {
            // add filler transaction with latest sequence
            QnTransaction<ApiFillerData> fillerTran(ApiCommand::updatePersistentSequence);
            fillerTran.peerID = key.peerID;
            fillerTran.persistentInfo.dbID = key.dbID;
            fillerTran.persistentInfo.sequence = latestSequence;
            result << QnUbjsonTransactionSerializer::instance()->serializedTransaction(fillerTran);
        }
    }
    
    return ErrorCode::ok;
}

#define QN_TRANSACTION_LOG_DATA_TYPES \
    (QnTranStateKey)\
    (QnTranState)\
    (QnTranStateResponse)\

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(QN_TRANSACTION_LOG_DATA_TYPES,  (binary)(json)(ubjson), _Fields)

} // namespace ec2
