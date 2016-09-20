#include "transaction_log.h"

#include <QtCore/QCryptographicHash>
#include <QtSql/QSqlQuery>
#include <QtSql/QSqlError>

#include "common/common_module.h"
#include "database/db_manager.h"
#include "transaction.h"
#include "transaction/ubjson_transaction_serializer.h"
#include <nx/utils/log/log.h>
#include "utils/common/synctime.h"
#include "nx/fusion/model_functions.h"
#include "nx_ec/data/api_discovery_data.h"

namespace ec2
{

static QnTransactionLog* globalInstance = nullptr;

QnTransactionLog::QnTransactionLog(detail::QnDbManager* db): m_dbManager(db)
{
    NX_ASSERT(!globalInstance);
    globalInstance = this;
    m_lastTimestamp = Timestamp::fromInteger(0);
    m_baseTime = Timestamp::fromInteger(0);
}

QnTransactionLog::~QnTransactionLog()
{
    NX_ASSERT(globalInstance == this);
    globalInstance = nullptr;
}

bool QnTransactionLog::clear()
{
    QSqlQuery query(m_dbManager->getDB());
    query.prepare("DELETE from transaction_log");
    if (!query.exec())
    {
        qWarning() << Q_FUNC_INFO << query.lastError().text();
        return false;
    }
    query.prepare("DELETE from transaction_sequence");
    if (!query.exec())
    {
        qWarning() << Q_FUNC_INFO << query.lastError().text();
        return false;
    }

    m_state.values.clear();
    m_updateHistory.clear();
    m_commitData.clear();

    m_lastTimestamp = Timestamp::fromInteger(qnSyncTime->currentMSecsSinceEpoch());
    m_baseTime = m_lastTimestamp;
    m_relativeTimer.restart();

    return true;
}

bool QnTransactionLog::init()
{
    QSqlQuery delQuery(m_dbManager->getDB());
    delQuery.prepare("DELETE FROM transaction_sequence \
                       WHERE NOT (transaction_sequence.peer_guid = ? AND transaction_sequence.db_guid = ?) \
                         AND NOT exists(select 1 from transaction_log tl where tl.peer_guid = transaction_sequence.peer_guid and tl.db_guid = transaction_sequence.db_guid)");
    delQuery.addBindValue(QnSql::serialized_field(qnCommon->moduleGUID()));
    delQuery.addBindValue(QnSql::serialized_field(m_dbManager->getID()));
    if (!delQuery.exec())
        qWarning() << Q_FUNC_INFO << delQuery.lastError().text();

    QSqlQuery query(m_dbManager->getDB());
    query.setForwardOnly(true);
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
    else
    {
        return false;
    }
    if (!seqFound) {
        // migrate from previous version. Init sequence table

        QSqlQuery query(m_dbManager->getDB());
        query.setForwardOnly(true);
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
        else
        {
            return false;
        }
    }

    QSqlQuery query2(m_dbManager->getDB());
    query2.setForwardOnly(true);
    query2.prepare("SELECT tran_guid, timestamp_hi, timestamp, peer_guid, db_guid FROM transaction_log");
    if (query2.exec()) {
        while (query2.next()) {
            QnUuid hash = QnUuid::fromRfc4122(query2.value("tran_guid").toByteArray());
            Timestamp timestamp;
            timestamp.sequence = query2.value("timestamp_hi").toLongLong();
            timestamp.ticks = query2.value("timestamp").toLongLong();
            QnUuid peerID = QnUuid::fromRfc4122(query2.value("peer_guid").toByteArray());
            QnUuid dbID = QnUuid::fromRfc4122(query2.value("db_guid").toByteArray());
            m_updateHistory.insert(hash, UpdateHistoryData(QnTranStateKey(peerID, dbID), timestamp));
        }
    }
    else
    {
        return false;
    }

    m_lastTimestamp = Timestamp::fromInteger(qnSyncTime->currentMSecsSinceEpoch());
    QSqlQuery queryTime(m_dbManager->getDB());
    queryTime.setForwardOnly(true);
    queryTime.prepare(
        R"sql(
        SELECT timestamp_hi, MAX(timestamp) FROM transaction_log 
        GROUP BY timestamp_hi ORDER BY timestamp_hi DESC LIMIT 1
        )sql");
    if( !queryTime.exec() )
        return false;
    if (queryTime.next()) {
        Timestamp readTimestamp;
        readTimestamp.sequence = queryTime.value(0).toLongLong();
        readTimestamp.ticks = queryTime.value(1).toLongLong();
        m_lastTimestamp = qMax(m_lastTimestamp, readTimestamp);
    }
    m_baseTime = m_lastTimestamp;
    m_relativeTimer.start();

    return true;
}

Timestamp QnTransactionLog::getTimeStamp()
{
    const qint64 absoluteTime = qnSyncTime->currentMSecsSinceEpoch();
    // TODO: transaction timestamp: add sequence
    Timestamp newTime = Timestamp::fromInteger(absoluteTime);

    QnMutexLocker lock( &m_timeMutex );
    if (newTime > m_lastTimestamp)
    {
        m_baseTime = m_lastTimestamp = newTime;
        m_relativeTimer.restart();
    }
    else
    {
        static const int TIME_SHIFT_DELTA = 1000;
        newTime = m_baseTime + m_relativeTimer.elapsed();
        if (newTime > m_lastTimestamp)
        {
            if (newTime > m_lastTimestamp + TIME_SHIFT_DELTA && newTime > absoluteTime + TIME_SHIFT_DELTA) {
                newTime -= TIME_SHIFT_DELTA; // try to reach absolute time
                m_baseTime = newTime;
                m_relativeTimer.restart();
            }
            m_lastTimestamp = newTime;
        }
        else {
            m_lastTimestamp++;
            m_baseTime = m_lastTimestamp;
            m_relativeTimer.restart();
        }
    }
    return m_lastTimestamp;
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

QnUuid QnTransactionLog::makeHash(const QByteArray& data1, const QByteArray& data2)
{
    QCryptographicHash hash(QCryptographicHash::Md5);
    hash.addData(data1);
    if (!data2.isEmpty())
        hash.addData(data2);
    return QnUuid::fromRfc4122(hash.result());
}

QnUuid QnTransactionLog::makeHash(const QByteArray &extraData, const ApiDiscoveryData &data)
{
    QCryptographicHash hash(QCryptographicHash::Md5);
    hash.addData(extraData);
    hash.addData(data.url.toUtf8());
    hash.addData(data.id.toString().toUtf8());
    return QnUuid::fromRfc4122(hash.result());
}

ErrorCode QnTransactionLog::updateSequence(const ApiUpdateSequenceData& data)
{
    detail::QnDbManager::QnDbTransactionLocker locker(dbManager(Qn::kSystemAccess).getTransaction());
    for(const ApiSyncMarkerRecord& record: data.markers)
    {
        NX_LOG( QnLog::EC2_TRAN_LOG, lit("update transaction sequence in log. key=%1 dbID=%2 dbSeq=%3").arg(record.peerID.toString()).arg(record.dbID.toString()).arg(record.sequence), cl_logDEBUG1);
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

ErrorCode QnTransactionLog::saveToDB(
    const QnAbstractTransaction& tran,
    const QnUuid& hash,
    const QByteArray& data)
{
    if (tran.isLocal())
        return ErrorCode::ok; // local transactions just changes DB without logging

    NX_LOG( QnLog::EC2_TRAN_LOG, lit("add transaction to log: %1 hash=%2").arg(tran.toString()).arg(hash.toString()), cl_logDEBUG1);

    NX_ASSERT(!tran.peerID.isNull(), Q_FUNC_INFO, "Transaction ID MUST be filled!");
    NX_ASSERT(!tran.persistentInfo.dbID.isNull(), Q_FUNC_INFO, "Transaction ID MUST be filled!");
    NX_ASSERT(tran.persistentInfo.sequence, Q_FUNC_INFO, "Transaction sequence MUST be filled!");
    if (tran.peerID == qnCommon->moduleGUID() && tran.persistentInfo.dbID == m_dbManager->instance()->getID())
        NX_ASSERT(tran.persistentInfo.timestamp > 0);

    QSqlQuery query(m_dbManager->getDB());
    query.prepare(
        R"sql(
        INSERT OR REPLACE INTO transaction_log(peer_guid, db_guid, sequence, timestamp_hi, timestamp, tran_guid, tran_data, tran_type)
        VALUES (?, ?, ?, ?, ?, ?, ?, ?)
        )sql");
    query.addBindValue(tran.peerID.toRfc4122());
    query.addBindValue(tran.persistentInfo.dbID.toRfc4122());
    query.addBindValue(tran.persistentInfo.sequence);
    query.addBindValue(tran.persistentInfo.timestamp.sequence);
    query.addBindValue(tran.persistentInfo.timestamp.ticks);
    query.addBindValue(hash.toRfc4122());
    query.addBindValue(data);
    query.addBindValue(tran.transactionType);
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

    NX_ASSERT ((tran.persistentInfo.timestamp > updateHistoryItr.value().timestamp) ||
            (tran.persistentInfo.timestamp == updateHistoryItr.value().timestamp && !(key < updateHistoryItr.value().updatedBy)));

    updateHistoryItr.value().timestamp = tran.persistentInfo.timestamp;
    updateHistoryItr.value().updatedBy = key;
    */
    m_commitData.updateHistory[hash] = UpdateHistoryData(key, tran.persistentInfo.timestamp);

    QnMutexLocker lock( &m_timeMutex );
    m_lastTimestamp = qMax(m_lastTimestamp, tran.persistentInfo.timestamp);
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
    QnReadLocker lock(&m_dbManager->getMutex());
    return m_state;
}

int QnTransactionLog::getLatestSequence(const QnTranStateKey& key) const
{
    QnReadLocker lock(&m_dbManager->getMutex());
    return m_state.values.value(key);
}

QnTransactionLog::ContainsReason QnTransactionLog::contains(const QnAbstractTransaction& tran, const QnUuid& hash) const
{

    QnTranStateKey key (tran.peerID, tran.persistentInfo.dbID);
    NX_ASSERT(tran.persistentInfo.sequence != 0);
    if (m_state.values.value(key) >= tran.persistentInfo.sequence) {
        NX_LOG( QnLog::EC2_TRAN_LOG,
            lit("Transaction log contains transaction %1 because of precessed seq: %2 >= %3").
            arg(tran.toString()).arg(m_state.values.value(key)).arg(tran.persistentInfo.sequence), cl_logDEBUG1 );
        return Reason_Sequence;
    }
    const auto itr = m_updateHistory.find(hash);
    if (itr == m_updateHistory.constEnd())
        return Reason_None;

    const auto lastTime = itr.value().timestamp;
    bool rez = lastTime > tran.persistentInfo.timestamp;
    if (lastTime == tran.persistentInfo.timestamp)
        rez = key < itr.value().updatedBy;
    if (rez) {
        NX_LOG( QnLog::EC2_TRAN_LOG,
            lm("Transaction log contains transaction %1 because of timestamp: %2 >= %3").
            arg(tran.toString()).str(lastTime).str(tran.persistentInfo.timestamp), cl_logDEBUG1 );
        return Reason_Timestamp;
    }
    else {
        return Reason_None;
    }
}

bool QnTransactionLog::contains(const QnTranState& state) const
{
    QnReadLocker lock(&m_dbManager->getMutex());
    for (auto itr = state.values.begin(); itr != state.values.end(); ++itr)
    {
        if (itr.value() > m_state.values.value(itr.key()))
            return false;
    }
    return true;
}

ErrorCode QnTransactionLog::getTransactionsAfter(
	const QnTranState& state,
    bool onlyCloudData,
	QList<QByteArray>& result)
{
    QString extraFilter;
    if (onlyCloudData)
        extraFilter = lit("AND tran_type = %1").arg(TransactionType::Cloud);

    QnReadLocker lock(&m_dbManager->getMutex());
    QMap <QnTranStateKey, int> tranLogSequence;
    for(auto itr = m_state.values.begin(); itr != m_state.values.end(); ++itr)
    {
        const QnTranStateKey& key = itr.key();
        QSqlQuery query(m_dbManager->getDB());
        query.setForwardOnly(true);
        query.prepare(lit("SELECT tran_data, sequence FROM transaction_log WHERE peer_guid = ? \
            AND db_guid = ? AND sequence > ? \
            %1 ORDER BY sequence").arg(extraFilter));
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
    query.setForwardOnly(true);
    query.prepare("SELECT peer_guid, db_guid, sequence from transaction_sequence");
    if (!query.exec())
        return ErrorCode::failure;

    QnTransaction<ApiUpdateSequenceData> syncMarkersTran(ApiCommand::updatePersistentSequence);
    while (query.next())
    {
        QnTranStateKey key(
			QnUuid::fromRfc4122(query.value(0).toByteArray()),
			QnUuid::fromRfc4122(query.value(1).toByteArray())
		);
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
    if (tran.persistentInfo.isNull())
    {
        if (!tran.isLocal())
            tran.persistentInfo.sequence = currentSequenceNoLock() + 1;
        tran.persistentInfo.dbID = m_dbManager->getID();
        tran.persistentInfo.timestamp = getTimeStamp();
    }
}

Timestamp QnTransactionLog::getTransactionLogTime() const
{
    QnMutexLocker lock( &m_timeMutex );
    return m_lastTimestamp;
}

void QnTransactionLog::setTransactionLogTime(Timestamp value)
{
    QnMutexLocker lock( &m_timeMutex );
    m_lastTimestamp = qMax(value, m_lastTimestamp);
}


} // namespace ec2
