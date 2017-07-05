#include "transaction_log.h"

#include <QtCore/QCryptographicHash>
#include <QtSql/QSqlQuery>
#include <QtSql/QSqlError>

#include "common/common_module.h"
#include <database/db_manager.h>
#include "transaction.h"
#include "transaction/ubjson_transaction_serializer.h"
#include <nx/utils/log/log.h>
#include "utils/common/synctime.h"
#include "nx/fusion/model_functions.h"
#include "nx_ec/data/api_discovery_data.h"
#include <common/static_common_module.h>

namespace ec2
{

QnTransactionLog::QnTransactionLog(
    detail::QnDbManager* db,
    QnUbjsonTransactionSerializer* tranSerializer)
:
    m_dbManager(db),
    m_tranSerializer(tranSerializer)
{
    m_lastTimestamp = Timestamp::fromInteger(0);
    m_baseTime = 0;
}

QnTransactionLog::~QnTransactionLog()
{
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

    if (!m_dbManager->updateId())
        return false;

    m_state.values.clear();
    m_updateHistory.clear();
    m_commitData.clear();

    m_baseTime = qnSyncTime->currentMSecsSinceEpoch();
    m_lastTimestamp = Timestamp::fromInteger(m_baseTime);
    m_relativeTimer.restart();

    return true;
}

bool QnTransactionLog::init()
{
    const auto& commonModule = m_dbManager->commonModule();
    QSqlQuery delQuery(m_dbManager->getDB());
    delQuery.prepare("DELETE FROM transaction_sequence \
                       WHERE NOT (transaction_sequence.peer_guid = ? AND transaction_sequence.db_guid = ?) \
                         AND NOT exists(select 1 from transaction_log tl where tl.peer_guid = transaction_sequence.peer_guid and tl.db_guid = transaction_sequence.db_guid)");
    delQuery.addBindValue(QnSql::serialized_field(commonModule->moduleGUID()));
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
            ApiPersistentIdData key(QnUuid::fromRfc4122(query.value(0).toByteArray()), QnUuid::fromRfc4122(query.value(1).toByteArray()));
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
                ApiPersistentIdData key(peerID, dbID);
                updateSequenceNoLock(peerID, dbID, sequence);
            }
        }
        else
        {
            return false;
        }
    }

    m_lastTimestamp = Timestamp::fromInteger(qnSyncTime->currentMSecsSinceEpoch());

    QSqlQuery query2(m_dbManager->getDB());
    query2.setForwardOnly(true);
    query2.prepare("SELECT tran_guid, timestamp_hi, timestamp, peer_guid, db_guid FROM transaction_log");
    if (query2.exec())
    {
        while (query2.next())
        {
            QnUuid hash = QnUuid::fromRfc4122(query2.value("tran_guid").toByteArray());
            Timestamp timestamp;
            timestamp.sequence = query2.value("timestamp_hi").toLongLong();
            timestamp.ticks = query2.value("timestamp").toLongLong();
            QnUuid peerID = QnUuid::fromRfc4122(query2.value("peer_guid").toByteArray());
            QnUuid dbID = QnUuid::fromRfc4122(query2.value("db_guid").toByteArray());
            m_updateHistory.insert(hash, UpdateHistoryData(ApiPersistentIdData(peerID, dbID), timestamp));

            m_lastTimestamp = qMax(m_lastTimestamp, timestamp);
        }
    }
    else
    {
        return false;
    }

    m_baseTime = m_lastTimestamp.ticks;
    m_relativeTimer.start();

    return true;
}

Timestamp QnTransactionLog::getTimeStamp()
{
    const qint64 absoluteTime = qnSyncTime->currentMSecsSinceEpoch();

    Timestamp newTime = Timestamp(m_lastTimestamp.sequence, absoluteTime);

    QnMutexLocker lock( &m_timeMutex );
    if (newTime > m_lastTimestamp)
    {
        m_lastTimestamp = newTime;
        m_baseTime = absoluteTime;
        m_relativeTimer.restart();
    }
    else
    {
        static const int TIME_SHIFT_DELTA = 1000;
        newTime.ticks = m_baseTime + m_relativeTimer.elapsed();
        if (newTime > m_lastTimestamp)
        {
            if (newTime > m_lastTimestamp + TIME_SHIFT_DELTA && newTime > absoluteTime + TIME_SHIFT_DELTA) {
                newTime -= TIME_SHIFT_DELTA; // try to reach absolute time
                m_baseTime = newTime.ticks;
                m_relativeTimer.restart();
            }
            m_lastTimestamp = newTime;
        }
        else {
            m_lastTimestamp++;
            m_baseTime = m_lastTimestamp.ticks;
            m_relativeTimer.restart();
        }
    }
    return m_lastTimestamp;
}

int QnTransactionLog::currentSequenceNoLock() const
{
    ApiPersistentIdData key (m_dbManager->commonModule()->moduleGUID(), m_dbManager->getID());
    return qMax(m_state.values.value(key), m_commitData.state.values.value(key));
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
    detail::QnDbManager::QnDbTransactionLocker
        locker(dbManager(m_dbManager, Qn::kSystemAccess).getTransaction());

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

ErrorCode QnTransactionLog::updateSequence(const QnAbstractTransaction& tran, TransactionLockType lockType)
{
    std::unique_ptr<detail::QnDbManager::QnAbstractTransactionLocker> locker;
    if (lockType == TransactionLockType::Regular)
        locker.reset(new detail::QnDbManager::QnDbTransactionLocker(m_dbManager->getTransaction()));
    else
        locker.reset(new detail::QnDbManager::QnLazyTransactionLocker(m_dbManager->getTransaction()));

    //NX_ASSERT(m_state.values.value(ApiPersistentIdData(tran.peerID, tran.persistentInfo.dbID)) <= tran.persistentInfo.sequence);
    if (m_state.values.value(ApiPersistentIdData(tran.peerID, tran.persistentInfo.dbID)) >= tran.persistentInfo.sequence)
    {
        if (lockType == TransactionLockType::Lazy)
            locker->commit(); //< prevent rollback previously saved data
        return ErrorCode::containsBecauseSequence;
    }

    NX_LOG(QnLog::EC2_TRAN_LOG, lit("update transaction sequence in log. key=%1 dbID=%2 dbSeq=%3")
        .arg(tran.peerID.toString())
        .arg(tran.persistentInfo.dbID.toString())
        .arg(tran.persistentInfo.sequence), cl_logDEBUG1);
    ErrorCode result = updateSequenceNoLock(tran.peerID, tran.persistentInfo.dbID, tran.persistentInfo.sequence);
    if (result != ErrorCode::ok)
        return result;
    if (locker->commit())
        return ErrorCode::ok;
    else
        return ErrorCode::dbError;
}

ErrorCode QnTransactionLog::updateSequenceNoLock(const QnUuid& peerID, const QnUuid& dbID, int sequence)
{
    ApiPersistentIdData key(peerID, dbID);
    if (m_state.values.value(key) >= sequence)
        return ErrorCode::ok;

    static const int kMaxSequenceHole = 50;
    int prevValue = std::max(m_commitData.state.values.value(key), m_state.values.value(key));
    if (sequence - prevValue > kMaxSequenceHole)
    {
        NX_WARNING(
            this,
            lm("Peer %1 has got suspicious sequence gap %1 --> %2 from peer %3")
            .arg(qnStaticCommon->moduleDisplayName(m_dbManager->commonModule()->moduleGUID()))
            .arg(prevValue)
            .arg(sequence)
            .arg(qnStaticCommon->moduleDisplayName(peerID)));
    }

    if (!m_updateSequenceQuery)
    {
        m_updateSequenceQuery.reset(new QSqlQuery(m_dbManager->getDB()));
        //query.prepare("INSERT OR REPLACE INTO transaction_sequence (peer_guid, db_guid, sequence) values (?, ?, ?)");
        m_updateSequenceQuery->prepare("INSERT OR REPLACE INTO transaction_sequence values (?, ?, ?)");
    }
    m_updateSequenceQuery->addBindValue(peerID.toRfc4122());
    m_updateSequenceQuery->addBindValue(dbID.toRfc4122());
    m_updateSequenceQuery->addBindValue(sequence);
    if (!m_updateSequenceQuery->exec())
    {
        qWarning() << Q_FUNC_INFO << m_updateSequenceQuery->lastError().text();
        return ErrorCode::failure;
    }
    m_commitData.state.values[key] = sequence;
    return ErrorCode::ok;
}

void QnTransactionLog::resetPreparedStatements()
{
    m_insTranQuery.reset();
    m_updateSequenceQuery.reset();
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
    if (tran.peerID == m_dbManager->commonModule()->moduleGUID() && tran.persistentInfo.dbID == m_dbManager->getID())
        NX_ASSERT(tran.persistentInfo.timestamp > 0);

    if (!m_insTranQuery)
    {
        m_insTranQuery.reset(new QSqlQuery(m_dbManager->getDB()));
        m_insTranQuery->prepare(
            R"sql(
            INSERT OR REPLACE INTO transaction_log(peer_guid, db_guid, sequence, timestamp_hi, timestamp, tran_guid, tran_data, tran_type)
            VALUES (?, ?, ?, ?, ?, ?, ?, ?)
            )sql");
    }


    m_insTranQuery->addBindValue(tran.peerID.toRfc4122());
    m_insTranQuery->addBindValue(tran.persistentInfo.dbID.toRfc4122());
    m_insTranQuery->addBindValue(tran.persistentInfo.sequence);
    m_insTranQuery->addBindValue(tran.persistentInfo.timestamp.sequence);
    m_insTranQuery->addBindValue(tran.persistentInfo.timestamp.ticks);
    m_insTranQuery->addBindValue(hash.toRfc4122());
    m_insTranQuery->addBindValue(data);
    m_insTranQuery->addBindValue(tran.transactionType);
    if (!m_insTranQuery->exec())
    {
        qWarning() << Q_FUNC_INFO << m_insTranQuery->lastError().text();
        return ErrorCode::failure;
    }
    #ifdef TRANSACTION_LOG_DEBUG
        qDebug() << "add record to transaction log. Transaction=" << toString(tran.command) << "timestamp=" << tran.timestamp << "producedOnCurrentPeer=" << (tran.peerID == commonModule()->moduleGUID());
    #endif

    ApiPersistentIdData key(tran.peerID, tran.persistentInfo.dbID);
    ErrorCode code = updateSequenceNoLock(tran.peerID, tran.persistentInfo.dbID, tran.persistentInfo.sequence);
    if (code != ErrorCode::ok)
        return code;

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

QVector<qint32> QnTransactionLog::getTransactionsState(const QVector<ApiPersistentIdData>& filter)
{
    QnReadLocker lock(&m_dbManager->getMutex());

    QVector<qint32> result;
    const auto& values = m_state.values;

    auto itrMap = values.begin();
    for (auto itrFilter = filter.begin(); itrFilter != filter.end(); ++itrFilter)
    {
        while (itrMap != values.end() && itrMap.key() < *itrFilter)
            ++itrMap;
        if (itrMap != values.end() && itrMap.key() == *itrFilter)
            result.push_back(itrMap.value());
        else
            result.push_back(0);
    }

    return result;
}

int QnTransactionLog::getLatestSequence(const ApiPersistentIdData& key) const
{
    QnReadLocker lock(&m_dbManager->getMutex());
    return m_state.values.value(key);
}

QnTransactionLog::ContainsReason QnTransactionLog::contains(const QnAbstractTransaction& tran, const QnUuid& hash) const
{

    ApiPersistentIdData key (tran.peerID, tran.persistentInfo.dbID);
    NX_ASSERT(tran.persistentInfo.sequence != 0);
    if (m_state.values.value(key) >= tran.persistentInfo.sequence) {
        NX_LOG( QnLog::EC2_TRAN_LOG,
            lit("Transaction log contains transaction %1, hash=%2 because of precessed seq: %3 >= %4").
            arg(tran.toString()).arg(hash.toString()).arg(m_state.values.value(key)).arg(tran.persistentInfo.sequence), cl_logDEBUG1 );
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
            lm("Transaction log contains transaction %1, hash=%2 because of timestamp: %3 >= %4").
            arg(tran.toString()).arg(hash.toString()).arg(lastTime).arg(tran.persistentInfo.timestamp), cl_logDEBUG1 );
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
    const QnTranState& filterState,
    bool onlyCloudData,
    QList<QByteArray>& result)
{
    QnReadLocker lock(&m_dbManager->getMutex());

    const QnTranState& stateToIterate = m_state;

    QnTransaction<ApiUpdateSequenceData> syncMarkersTran(
        ApiCommand::updatePersistentSequence,
        m_dbManager->commonModule()->moduleGUID());

    QString extraFilter;
    if (onlyCloudData)
        extraFilter = lit("AND tran_type = %1").arg(TransactionType::Cloud);

    for (auto itr = stateToIterate.values.begin(); itr != stateToIterate.values.end(); ++itr)
    {
        const ApiPersistentIdData& key = itr.key();
        qint32 querySequence = filterState.values.value(key);
        QSqlQuery query(m_dbManager->getDB());
        query.setForwardOnly(true);
        query.prepare(lit("SELECT tran_data, sequence FROM transaction_log WHERE peer_guid = ? \
            AND db_guid = ? AND sequence > ? \
            %1 ORDER BY sequence").arg(extraFilter));
        query.addBindValue(key.id.toRfc4122());
        query.addBindValue(key.persistentId.toRfc4122());
        query.addBindValue(querySequence);
        if (!query.exec())
            return ErrorCode::failure;

        qint32 lastSelectedSequence = 0;
        while (query.next())
        {
            result << query.value(0).toByteArray();
            lastSelectedSequence = query.value(1).toInt();
        }
        qint32 latestSequence = m_state.values.value(itr.key());
        if (latestSequence > lastSelectedSequence)
        {
            // add filler transaction with latest sequence
            ApiSyncMarkerRecord record;
            record.peerID = key.id;
            record.dbID = key.persistentId;
            record.sequence = latestSequence;
            syncMarkersTran.params.markers.push_back(record);
        }
    }

    result << m_tranSerializer->serializedTransaction(syncMarkersTran);
    return ErrorCode::ok;
}

ErrorCode QnTransactionLog::getExactTransactionsAfter(
    QnTranState* inOutState,
    bool onlyCloudData,
    QList<QByteArray>& result,
    int maxDataSize,
    bool* outIsFinished)
{
    *outIsFinished = false;
    QnReadLocker lock(&m_dbManager->getMutex());

    QnTransaction<ApiUpdateSequenceData> syncMarkersTran(
        ApiCommand::updatePersistentSequence,
        m_dbManager->commonModule()->moduleGUID());

    QString extraFilter;
    if (onlyCloudData)
        extraFilter = lit("AND tran_type = %1").arg(TransactionType::Cloud);

    //QMap <ApiPersistentIdData, int> tranLogSequence;
    for (auto itr = inOutState->values.begin(); itr != inOutState->values.end(); ++itr)
    {
        const ApiPersistentIdData& key = itr.key();
        qint32 querySequence = itr.value();
        const qint32 latestSequence = m_state.values.value(itr.key());
        if (querySequence >= latestSequence)
            continue;

        QSqlQuery query(m_dbManager->getDB());
        query.setForwardOnly(true);
        query.prepare(lit("SELECT tran_data, sequence FROM transaction_log WHERE peer_guid = ? \
            AND db_guid = ? AND sequence > ? \
            %1 ORDER BY sequence").arg(extraFilter));
        query.addBindValue(key.id.toRfc4122());
        query.addBindValue(key.persistentId.toRfc4122());
        query.addBindValue(querySequence);
        if (!query.exec())
            return ErrorCode::failure;

        qint32 lastSelectedSequence = 0;
        while (query.next())
        {
            result << query.value(0).toByteArray();
            lastSelectedSequence = query.value(1).toInt();
            maxDataSize -= result.last().size();
            if (maxDataSize <= 0)
            {
                itr.value() = std::max(querySequence, lastSelectedSequence);
                return ErrorCode::ok;
            }
        }
        itr.value() = std::max(querySequence, latestSequence);
        if (latestSequence > lastSelectedSequence)
        {
            if (latestSequence > querySequence)
            {
                syncMarkersTran.peerID = key.id;
                syncMarkersTran.persistentInfo.dbID = key.persistentId;
                syncMarkersTran.persistentInfo.sequence = latestSequence;
                result << m_tranSerializer->serializedTransaction(syncMarkersTran);
            }
        }
    }
    *outIsFinished = true;
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
