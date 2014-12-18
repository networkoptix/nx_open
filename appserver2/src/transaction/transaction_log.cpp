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
    QSqlQuery delQuery(m_dbManager->getDB());
    delQuery.prepare("DELETE FROM transaction_sequence WHERE not exists(select 1 from transaction_log tl where tl.peer_guid = transaction_sequence.peer_guid and tl.db_guid = transaction_sequence.db_guid)");
    delQuery.exec();

    QSqlQuery query(m_dbManager->getDB());
    bool seqFound = false;
    query.prepare("SELECT peer_guid, db_guid, sequence FROM transaction_sequence");
    if (query.exec()) {
        while (query.next()) 
        {
            seqFound = true;
            QnTranStateKey key(QnUuid::fromRfc4122(query.value(0).toByteArray()), QnUuid::fromRfc4122(query.value(1).toByteArray()));
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
                QnUuid peerID = QnUuid::fromRfc4122(query.value(0).toByteArray());
                QnUuid dbID = QnUuid::fromRfc4122(query.value(1).toByteArray());
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
            QnUuid hash = QnUuid::fromRfc4122(query2.value("tran_guid").toByteArray());
            qint64 timestamp = query2.value("timestamp").toLongLong();
            QnUuid peerID = QnUuid::fromRfc4122(query2.value("peer_guid").toByteArray());
            QnUuid dbID = QnUuid::fromRfc4122(query2.value("db_guid").toByteArray());
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

int QnTransactionLog::currentSequenceNoLock() const
{
    QnTranStateKey key (qnCommon->moduleGUID(), m_dbManager->getID());
    return qMax(m_state.values.value(key), m_commitData.state.values.value(key));
}

QnTransactionLog* QnTransactionLog::instance()
{
    return globalInstance;
}

QnUuid QnTransactionLog::makeHash(const QByteArray& data1, const QByteArray& data2) const
{
    QCryptographicHash hash(QCryptographicHash::Md5);
    hash.addData(data1);
    if (!data2.isEmpty())
        hash.addData(data2);
    return QnUuid::fromRfc4122(hash.result());
}

QnUuid QnTransactionLog::transactionHash(const ApiResourceParamWithRefData& param) const
{
    QCryptographicHash hash(QCryptographicHash::Md5);
    hash.addData("res_params");
    hash.addData(param.resourceId.toRfc4122());
    hash.addData(param.name.toUtf8());
    return QnUuid::fromRfc4122(hash.result());
}

QnUuid QnTransactionLog::makeHash(const QString& extraData, const ApiCameraBookmarkTagDataList& data) const {
    QCryptographicHash hash(QCryptographicHash::Md5);
    hash.addData(extraData.toUtf8());
    for(const ApiCameraBookmarkTagData tag: data)
        hash.addData(tag.name.toUtf8());
    return QnUuid::fromRfc4122(hash.result());
}

QnUuid QnTransactionLog::makeHash(const QString &extraData, const ApiDiscoveryData &data) const {
    QCryptographicHash hash(QCryptographicHash::Md5);
    hash.addData(extraData.toUtf8());
    hash.addData(data.url.toUtf8());
    hash.addData(data.id.toString().toUtf8());
    return QnUuid::fromRfc4122(hash.result());
}

ErrorCode QnTransactionLog::updateSequence(const ApiUpdateSequenceData& data)
{
    QnDbManager::QnDbTransactionLocker locker(dbManager->getTransaction());
    for(const ApiSyncMarkerRecord& record: data.markers) 
    {
        NX_LOG( QnLog::EC2_TRAN_LOG, lit("update transaction sequence in log. key=%1 dbID=%2 dbSeq=%4").arg(record.peerID).arg(record.dbID).arg(record.sequence), cl_logDEBUG1);
        ErrorCode result = updateSequenceNoLock(record.peerID, record.dbID, record.sequence);
        if (result != ErrorCode::ok)
            return result;
    }
    if (locker.commit())
        return ErrorCode::ok;
    else
        return ErrorCode::dbError;
}

ErrorCode QnTransactionLog::updateSequenceNoLock(const QnUuid& peerID, const QnUuid& dbID, int sequence)
{
    QnTranStateKey key(peerID, dbID);
    if (m_state.values.value(key) >= sequence)
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
    m_commitData.state.values[key] = sequence;
    return ErrorCode::ok;
}

ErrorCode QnTransactionLog::saveToDB(const QnAbstractTransaction& tran, const QnUuid& hash, const QByteArray& data)
{
    if (tran.isLocal)
        return ErrorCode::ok; // local transactions just changes DB without logging

    NX_LOG( QnLog::EC2_TRAN_LOG, lit("add transaction to log: %1 hash=%2").arg(tran.toString()).arg(hash.toString()), cl_logDEBUG1);

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

    /*
    auto updateHistoryItr = m_updateHistory.find(hash);
    if (updateHistoryItr == m_updateHistory.end())
        updateHistoryItr = m_updateHistory.insert(hash, UpdateHistoryData());

    assert ((tran.persistentInfo.timestamp > updateHistoryItr.value().timestamp) ||
            (tran.persistentInfo.timestamp == updateHistoryItr.value().timestamp && !(key < updateHistoryItr.value().updatedBy)));

    updateHistoryItr.value().timestamp = tran.persistentInfo.timestamp;
    updateHistoryItr.value().updatedBy = key;
    */
    m_commitData.updateHistory[hash] = UpdateHistoryData(key, tran.persistentInfo.timestamp);

    QMutexLocker lock(&m_timeMutex);
    if (tran.persistentInfo.timestamp > m_currentTime + m_relativeTimer.elapsed()) {
        m_currentTime = m_lastTimestamp = tran.persistentInfo.timestamp;
        m_relativeTimer.restart();
    }

    return ErrorCode::ok;
}

void QnTransactionLog::beginTran()
{
    m_commitData.clear();
}

void QnTransactionLog::commit()
{
    for (auto itr = m_commitData.state.values.constBegin(); itr != m_commitData.state.values.constEnd(); ++itr)
        m_state.values[itr.key()] = itr.value();

    for (auto itr = m_commitData.updateHistory.constBegin(); itr != m_commitData.updateHistory.constEnd(); ++itr)
        m_updateHistory[itr.key()] = itr.value();
    
    m_commitData.clear();
}

void QnTransactionLog::rollback()
{
    m_commitData.clear();
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

QnTransactionLog::ContainsReason QnTransactionLog::contains(const QnAbstractTransaction& tran, const QnUuid& hash) const
{

    QnTranStateKey key (tran.peerID, tran.persistentInfo.dbID);
    Q_ASSERT(tran.persistentInfo.sequence != 0);
    if (m_state.values.value(key) >= tran.persistentInfo.sequence) {
#ifdef _DEBUG
        qDebug() << "Transaction log contains transaction " << tran.toString() << 
            "because of precessed seq:" << m_state.values.value(key) << ">=" << tran.persistentInfo.sequence;
#endif
        NX_LOG( lit("Transaction log contains transaction %1 because of precessed seq: %2 >= %3").
            arg(tran.toString()).arg(m_state.values.value(key)).arg(tran.persistentInfo.sequence), cl_logDEBUG1 );
        return Reason_Sequence;
    }
    const auto itr = m_updateHistory.find(hash);
    if (itr == m_updateHistory.constEnd())
        return Reason_None;

    const qint64 lastTime = itr.value().timestamp;
    bool rez = lastTime > tran.persistentInfo.timestamp;
    if (lastTime == tran.persistentInfo.timestamp)
        rez = key < itr.value().updatedBy;
    if (rez) {
#ifdef _DEBUG
        qDebug() << "Transaction log contains transaction " << tran.toString() << 
            "because of timestamp:" << lastTime << ">=" << tran.persistentInfo.timestamp;
#endif
        NX_LOG( lit("Transaction log contains transaction %1 because of timestamp: %2 >= %3").
            arg(tran.toString()).arg(lastTime).arg(tran.persistentInfo.timestamp), cl_logDEBUG1 );
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
    for(auto itr = m_state.values.begin(); itr != m_state.values.end(); ++itr)
    {
        const QnTranStateKey& key = itr.key();
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
        QnTranStateKey key(QnUuid::fromRfc4122(query.value(0).toByteArray()), QnUuid::fromRfc4122(query.value(1).toByteArray()));
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

void QnTransactionLog::fillPersistentInfo(QnAbstractTransaction& tran)
{
    if (tran.persistentInfo.isNull()) {
        if (!tran.isLocal)
            tran.persistentInfo.sequence = currentSequenceNoLock() + 1;
        tran.persistentInfo.dbID = m_dbManager->getID();
        tran.persistentInfo.timestamp = getTimeStamp();
    }
}


} // namespace ec2
