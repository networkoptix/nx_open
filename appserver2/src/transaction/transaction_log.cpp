#include "transaction_log.h"

#include <QtCore/QCryptographicHash>
#include <QtSql/QSqlQuery>
#include <QtSql/QSqlError>

#include "common/common_module.h"
#include "database/db_manager.h"
#include "transaction.h"
#include "utils/common/log.h"
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
    bool seqFound = false;
    query.prepare("SELECT peer_guid, db_guid, sequence FROM transaction_sequence");
    if (query.exec()) {
        while (query.next()) 
        {
            seqFound = true;
            QnTranStateKey key(QUuid::fromRfc4122(query.value(0).toByteArray()), QUuid::fromRfc4122(query.value(1).toByteArray()));
            m_state.values.insert(key, query.value(2).toInt());
        }
    }
    if (!seqFound) {
        // migrate from previous version. Init sequence table

        QSqlQuery query(m_dbManager->getDB());
        query.prepare("SELECT peer_guid, db_guid, max(sequence) as sequence FROM transaction_log GROUP BY peer_guid, db_guid");
        if (query.exec()) {
            while (query.next()) 
            {
                QUuid peerID = QUuid::fromRfc4122(query.value(0).toByteArray());
                QUuid dbID = QUuid::fromRfc4122(query.value(1).toByteArray());
                int sequence = query.value(2).toInt();
                QnTranStateKey key(peerID, dbID);
                updateSequenceNoLock(peerID, dbID, sequence);
            }
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
    m_lastTimestamp = m_currentTime;
    m_relativeTimer.start();

    QSqlQuery querySequence(m_dbManager->getDB());
    int startSequence = 1;
    querySequence.prepare("SELECT sequence FROM transaction_sequence where peer_guid = ? and db_guid = ?");
    querySequence.addBindValue(qnCommon->moduleGUID().toRfc4122());
    querySequence.addBindValue(m_dbManager->getID().toRfc4122());
    if (querySequence.exec() && querySequence.next())
        startSequence = querySequence.value(0).toInt() + 1;
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

ErrorCode QnTransactionLog::updateSequence(const ApiUpdateSequenceData& data)
{
    QnDbManager::Locker locker(dbManager);
    foreach(const ApiSyncMarkerRecord& record, data.markers) 
    {
        ErrorCode result = updateSequenceNoLock(record.peerID, record.dbID, record.sequence);
        if (result != ErrorCode::ok)
            return result;
    }
    locker.commit();
    return ErrorCode::ok;
}

ErrorCode QnTransactionLog::updateSequenceNoLock(const QUuid& peerID, const QUuid& dbID, int sequence)
{
    QnTranStateKey key(peerID, dbID);
    if (m_state.values[key] >= sequence)
        return ErrorCode::ok;

    QSqlQuery query(m_dbManager->getDB());
    //query.prepare("INSERT OR REPLACE INTO transaction_sequence (peer_guid, db_guid, sequence) values (?, ?, ?)");
    query.prepare("INSERT OR REPLACE INTO transaction_sequence values (?, ?, ?)");
    query.addBindValue(peerID.toRfc4122());
    query.addBindValue(dbID.toRfc4122());
    query.addBindValue(sequence);
    if (!query.exec()) {
        qWarning() << Q_FUNC_INFO << query.lastError().text();
        return ErrorCode::failure;
    }
    m_state.values[key] = sequence;

    return ErrorCode::ok;
}

ErrorCode QnTransactionLog::saveToDB(const QnAbstractTransaction& tran, const QUuid& hash, const QByteArray& data)
{
    if (tran.isLocal)
        return ErrorCode::ok; // local transactions just changes DB without logging

#ifdef TRANSACTION_MESSAGE_BUS_DEBUG
    NX_LOG( lit("add transaction to log %1 command=%2 db seq=%3 timestamp=%4").arg(tran.peerID.toString()).arg(ApiCommand::toString(tran.command)).
        arg(tran.persistentInfo.sequence).arg(tran.persistentInfo.timestamp), cl_logDEBUG1 );
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
    #ifdef TRANSACTION_LOG_DEBUG
        qDebug() << "add record to transaction log. Transaction=" << toString(tran.command) << "timestamp=" << tran.timestamp << "producedOnCurrentPeer=" << (tran.peerID == qnCommon->moduleGUID());
    #endif

    QnTranStateKey key(tran.peerID, tran.persistentInfo.dbID);
    ErrorCode code = updateSequenceNoLock(tran.peerID, tran.persistentInfo.dbID, tran.persistentInfo.sequence);
    if (code != ErrorCode::ok)
        return code;

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

int QnTransactionLog::getLatestSequence(const QnTranStateKey& key) const
{
    QReadLocker lock(&m_dbManager->getMutex());
    return m_state.values.value(key);
}

QnTransactionLog::ContainsReason QnTransactionLog::contains(const QnAbstractTransaction& tran, const QUuid& hash) const
{

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

bool QnTransactionLog::contains(const QnTranState& state) const
{
    QReadLocker lock(&m_dbManager->getMutex());
    for (auto itr = state.values.begin(); itr != state.values.end(); ++itr)
    {
        if (itr.value() > m_state.values.value(itr.key()))
            return false;
    }
    return true;
}

ErrorCode QnTransactionLog::getTransactionsAfter(const QnTranState& state, QList<QByteArray>& result)
{
    QReadLocker lock(&m_dbManager->getMutex());
    QMap <QnTranStateKey, int> tranLogSequence;
    foreach(const QnTranStateKey& key, m_state.values.keys())
    {
        QSqlQuery query(m_dbManager->getDB());
        query.prepare("SELECT tran_data, sequence FROM transaction_log WHERE peer_guid = ? and db_guid = ? and sequence > ?  order by sequence");
        query.addBindValue(key.peerID.toRfc4122());
        query.addBindValue(key.dbID.toRfc4122());
        query.addBindValue(state.values.value(key));
        if (!query.exec())
            return ErrorCode::failure;
        
        while (query.next()) {
            result << query.value(0).toByteArray();
            tranLogSequence[key] = query.value(1).toInt();
        }
    }

    // join fillter transactions to update state to the latest available sequences
    QSqlQuery query(m_dbManager->getDB());
    query.prepare("SELECT peer_guid, db_guid, sequence from transaction_sequence");
    if (!query.exec())
        return ErrorCode::failure;
    
    QnTransaction<ApiUpdateSequenceData> syncMarkersTran(ApiCommand::updatePersistentSequence);
    while (query.next()) 
    {
        QnTranStateKey key(QUuid::fromRfc4122(query.value(0).toByteArray()), QUuid::fromRfc4122(query.value(1).toByteArray()));
        int latestSequence =  query.value(2).toInt();
        if (latestSequence > tranLogSequence[key]) {
            // add filler transaction with latest sequence
            ApiSyncMarkerRecord record;
            record.peerID = key.peerID;
            record.dbID = key.dbID;
            record.sequence = latestSequence;
            syncMarkersTran.params.markers.push_back(record);
        }
    }
    result << QnUbjsonTransactionSerializer::instance()->serializedTransaction(syncMarkersTran);
    
    return ErrorCode::ok;
}

} // namespace ec2
