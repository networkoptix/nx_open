#include "transaction_log.h"

#include <QtCore/QCryptographicHash>
#include <QtSql/QSqlQuery>
#include <QtSql/QSqlError>

#include <common/common_module.h>
#include <database/db_manager.h>
#include <transaction/transaction.h>
#include <transaction/ubjson_transaction_serializer.h>
#include <utils/common/synctime.h>

#include <nx/fusion/model_functions.h>
#include <nx/utils/log/log.h>

using namespace nx;

namespace ec2 {

QnTransactionLog::QnTransactionLog(
    detail::QnDbManager* db,
    QnUbjsonTransactionSerializer* tranSerializer)
    :
    m_dbManager(db),
    m_tranSerializer(tranSerializer),
    m_insertTransactionQuery(db->queryCachePool()),
    m_updateSequenceQuery(db->queryCachePool())
{
    m_lastTimestamp = vms::api::Timestamp::fromInteger(0);
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
    m_lastTimestamp = vms::api::Timestamp::fromInteger(m_baseTime);
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
            vms::api::PersistentIdData key(QnUuid::fromRfc4122(query.value(0).toByteArray()), QnUuid::fromRfc4122(query.value(1).toByteArray()));
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
                vms::api::PersistentIdData key(peerID, dbID);
                updateSequenceNoLock(peerID, dbID, sequence);
            }
        }
        else
        {
            return false;
        }
    }

    m_lastTimestamp = vms::api::Timestamp::fromInteger(qnSyncTime->currentMSecsSinceEpoch());

    QSqlQuery query2(m_dbManager->getDB());
    query2.setForwardOnly(true);
    query2.prepare("SELECT tran_guid, timestamp_hi, timestamp, peer_guid, db_guid FROM transaction_log");
    if (query2.exec())
    {
        while (query2.next())
        {
            QnUuid hash = QnUuid::fromRfc4122(query2.value("tran_guid").toByteArray());
            vms::api::Timestamp timestamp;
            timestamp.sequence = query2.value("timestamp_hi").toLongLong();
            timestamp.ticks = query2.value("timestamp").toLongLong();
            QnUuid peerID = QnUuid::fromRfc4122(query2.value("peer_guid").toByteArray());
            QnUuid dbID = QnUuid::fromRfc4122(query2.value("db_guid").toByteArray());
            m_updateHistory.insert(hash, UpdateHistoryData(vms::api::PersistentIdData(peerID, dbID), timestamp));

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

vms::api::Timestamp QnTransactionLog::getTimeStamp()
{
    const qint64 absoluteTime = qnSyncTime->currentMSecsSinceEpoch();

    vms::api::Timestamp newTime = vms::api::Timestamp(m_lastTimestamp.sequence, absoluteTime);

    QnMutexLocker lock(&m_timeMutex);
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
    vms::api::PersistentIdData key (m_dbManager->commonModule()->moduleGUID(), m_dbManager->getID());
    return qMax(m_state.values.value(key), m_commitData.state.values.value(key));
}

ErrorCode QnTransactionLog::updateSequence(const nx::vms::api::UpdateSequenceData& data)
{
    detail::QnDbManager::QnDbTransactionLocker
        locker(dbManager(m_dbManager, Qn::kSystemAccess).getTransaction());

    for(const nx::vms::api::SyncMarkerRecordData& record: data.markers)
    {
        NX_DEBUG(QnLog::EC2_TRAN_LOG.join(this),
            lm("update transaction sequence in log. key=%1 dbID=%2 dbSeq=%3"),
            record.peerID.toString(), record.dbID.toString(), record.sequence);
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

    //NX_ASSERT(m_state.values.value(vms::api::PersistentIdData(tran.peerID, tran.persistentInfo.dbID)) <= tran.persistentInfo.sequence);
    if (m_state.values.value(vms::api::PersistentIdData(tran.peerID, tran.persistentInfo.dbID)) >= tran.persistentInfo.sequence)
    {
        if (lockType == TransactionLockType::Lazy)
            locker->commit(); //< prevent rollback previously saved data
        return ErrorCode::containsBecauseSequence;
    }

    NX_DEBUG(QnLog::EC2_TRAN_LOG, lit("update transaction sequence in log. key=%1 dbID=%2 dbSeq=%3")
        .arg(tran.peerID.toString())
        .arg(tran.persistentInfo.dbID.toString())
        .arg(tran.persistentInfo.sequence));
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
    vms::api::PersistentIdData key(peerID, dbID);
    if (m_state.values.value(key) >= sequence)
        return ErrorCode::ok;

#if 0
    static const int kMaxSequenceHole = 50;
    int prevValue = std::max(m_commitData.state.values.value(key), m_state.values.value(key));
    if (sequence - prevValue > kMaxSequenceHole)
    {
        NX_WARNING(
            this,
            lm("Peer %1 has got suspicious sequence gap %2 --> %3 from peer %4")
            .arg(qnStaticCommon->moduleDisplayName(m_dbManager->commonModule()->moduleGUID()))
            .arg(prevValue)
            .arg(sequence)
            .arg(qnStaticCommon->moduleDisplayName(peerID)));
    }
#endif

    const auto query = m_updateSequenceQuery.get(m_dbManager->getDB(),
        "INSERT OR REPLACE INTO transaction_sequence values (?, ?, ?)");

    query->addBindValue(peerID.toRfc4122());
    query->addBindValue(dbID.toRfc4122());
    query->addBindValue(sequence);

    if (!query->exec())
    {
        qWarning() << Q_FUNC_INFO << query->lastError().text();
        return ErrorCode::dbError;
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

    NX_DEBUG(QnLog::EC2_TRAN_LOG.join(this), lm("add transaction to log: %1 hash=%2"),
        tran.toString(), hash.toString());

    NX_ASSERT(!tran.peerID.isNull(), Q_FUNC_INFO, "Transaction ID MUST be filled!");
    NX_ASSERT(!tran.persistentInfo.dbID.isNull(), Q_FUNC_INFO, "Transaction ID MUST be filled!");
    NX_ASSERT(tran.persistentInfo.sequence, Q_FUNC_INFO, "Transaction sequence MUST be filled!");
    if (tran.peerID == m_dbManager->commonModule()->moduleGUID() && tran.persistentInfo.dbID == m_dbManager->getID())
        NX_ASSERT(tran.persistentInfo.timestamp > 0);

    const auto query = m_insertTransactionQuery.get(m_dbManager->getDB(), R"sql(
        INSERT OR REPLACE INTO transaction_log(
            peer_guid, db_guid, sequence, timestamp_hi, timestamp, tran_guid, tran_data, tran_type
        ) VALUES (
            ?, ?, ?, ?, ?, ?, ?, ?
        ))sql");

    query->addBindValue(tran.peerID.toRfc4122());
    query->addBindValue(tran.persistentInfo.dbID.toRfc4122());
    query->addBindValue(tran.persistentInfo.sequence);
    query->addBindValue(tran.persistentInfo.timestamp.sequence);
    query->addBindValue(tran.persistentInfo.timestamp.ticks);
    query->addBindValue(hash.toRfc4122());
    query->addBindValue(data);
    query->addBindValue(tran.transactionType);

    if (!query->exec())
    {
        qWarning() << Q_FUNC_INFO << query->lastError().text();
        return ErrorCode::dbError;
    }

    #ifdef TRANSACTION_LOG_DEBUG
        qDebug() << "add record to transaction log. Transaction=" << toString(tran.command) << "timestamp=" << tran.timestamp << "producedOnCurrentPeer=" << (tran.peerID == commonModule()->moduleGUID());
    #endif

    vms::api::PersistentIdData key(tran.peerID, tran.persistentInfo.dbID);
    ErrorCode code = updateSequenceNoLock(tran.peerID, tran.persistentInfo.dbID, tran.persistentInfo.sequence);
    if (code != ErrorCode::ok)
        return code;

    m_commitData.updateHistory[hash] = UpdateHistoryData(key, tran.persistentInfo.timestamp);

    QnMutexLocker lock(&m_timeMutex);
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

nx::vms::api::TranState QnTransactionLog::getTransactionsState()
{
    QnReadLocker lock(&m_dbManager->getMutex());
    return m_state;
}

QVector<qint32> QnTransactionLog::getTransactionsState(const QVector<vms::api::PersistentIdData>& filter)
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

int QnTransactionLog::getLatestSequence(const vms::api::PersistentIdData& key) const
{
    QnReadLocker lock(&m_dbManager->getMutex());
    return m_state.values.value(key);
}

QnTransactionLog::ContainsReason QnTransactionLog::contains(const QnAbstractTransaction& tran, const QnUuid& hash) const
{

    vms::api::PersistentIdData key (tran.peerID, tran.persistentInfo.dbID);
    NX_ASSERT(tran.persistentInfo.sequence != 0);
    if (m_state.values.value(key) >= tran.persistentInfo.sequence) {
        NX_DEBUG(QnLog::EC2_TRAN_LOG, lit("Transaction log contains transaction %1, hash=%2 because of precessed seq: %3 >= %4").
            arg(tran.toString()).arg(hash.toString()).arg(m_state.values.value(key)).arg(tran.persistentInfo.sequence));
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
        NX_DEBUG(QnLog::EC2_TRAN_LOG, lm("Transaction log contains transaction %1, hash=%2 because of timestamp: %3 >= %4").
            arg(tran.toString()).arg(hash.toString()).arg(lastTime).arg(tran.persistentInfo.timestamp));
        return Reason_Timestamp;
    }
    else {
        return Reason_None;
    }
}

bool QnTransactionLog::contains(const nx::vms::api::TranState& state) const
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
    const nx::vms::api::TranState& filterState,
    bool onlyCloudData,
    QList<QByteArray>& result)
{
    QnReadLocker lock(&m_dbManager->getMutex());

    const nx::vms::api::TranState& stateToIterate = m_state;

    QnTransaction<nx::vms::api::UpdateSequenceData> syncMarkersTran(
        ApiCommand::updatePersistentSequence,
        m_dbManager->commonModule()->moduleGUID());

    QString extraFilter;
    if (onlyCloudData)
        extraFilter = lit("AND tran_type = %1").arg(TransactionType::Cloud);

    for (auto itr = stateToIterate.values.begin(); itr != stateToIterate.values.end(); ++itr)
    {
        const vms::api::PersistentIdData& key = itr.key();
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
            return ErrorCode::dbError;

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
            nx::vms::api::SyncMarkerRecordData record;
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
    nx::vms::api::TranState* inOutState,
    bool onlyCloudData,
    QList<QByteArray>& result,
    int maxDataSize,
    bool* outIsFinished)
{
    *outIsFinished = false;
    QnReadLocker lock(&m_dbManager->getMutex());

    QnTransaction<nx::vms::api::UpdateSequenceData> syncMarkersTran(
        ApiCommand::updatePersistentSequence,
        m_dbManager->commonModule()->moduleGUID());

    QString extraFilter;
    if (onlyCloudData)
        extraFilter = lit("AND tran_type = %1").arg(TransactionType::Cloud);

    //QMap <vms::api::PersistentIdData, int> tranLogSequence;
    for (auto itr = inOutState->values.begin(); itr != inOutState->values.end(); ++itr)
    {
        const vms::api::PersistentIdData& key = itr.key();
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
            return ErrorCode::dbError;

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

vms::api::Timestamp QnTransactionLog::getTransactionLogTime() const
{
    QnMutexLocker lock(&m_timeMutex);
    return m_lastTimestamp;
}

void QnTransactionLog::setTransactionLogTime(vms::api::Timestamp value)
{
    QnMutexLocker lock(&m_timeMutex);
    m_lastTimestamp = qMax(value, m_lastTimestamp);
}

QnTransactionLog::UpdateHistoryData::UpdateHistoryData(
    const nx::vms::api::PersistentIdData& updatedBy,
    const nx::vms::api::Timestamp& timestamp)
    :
    updatedBy(updatedBy),
    timestamp(timestamp)
{
}

} // namespace ec2
