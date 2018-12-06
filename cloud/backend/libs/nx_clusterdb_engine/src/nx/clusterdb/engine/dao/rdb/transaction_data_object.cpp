#include "transaction_data_object.h"

#include <QtSql/QSqlError>
#include <QtSql/QSqlQuery>

#include <nx/utils/log/log.h>

namespace nx::clusterdb::engine::dao::rdb {

TransactionDataObject::TransactionDataObject(int transactionFormatVersion):
    m_transactionFormatVersion(transactionFormatVersion)
{
}

nx::sql::DBResult TransactionDataObject::insertOrReplaceTransaction(
    nx::sql::QueryContext* queryContext,
    const TransactionData& tran)
{
    QSqlQuery saveTranQuery(*queryContext->connection()->qtSqlConnection());
    saveTranQuery.prepare(
        R"sql(
        REPLACE INTO transaction_log(system_id, peer_guid, db_guid, sequence,
                                     timestamp_hi, timestamp, tran_hash, tran_data)
        VALUES (?, ?, ?, ?, ?, ?, ?, ?)
        )sql");
    saveTranQuery.addBindValue(tran.systemId.c_str());
    saveTranQuery.addBindValue(tran.header.peerID.toSimpleString());
    saveTranQuery.addBindValue(tran.header.persistentInfo.dbID.toSimpleString());
    saveTranQuery.addBindValue(tran.header.persistentInfo.sequence);
    saveTranQuery.addBindValue(tran.header.persistentInfo.timestamp.sequence);
    saveTranQuery.addBindValue(tran.header.persistentInfo.timestamp.ticks);
    saveTranQuery.addBindValue(QLatin1String(tran.hash));
    saveTranQuery.addBindValue(tran.ubjsonSerializedTransaction);
    if (!saveTranQuery.exec())
    {
        const auto str = saveTranQuery.lastError().text();

        NX_WARNING(QnLog::EC2_TRAN_LOG.join(this),
            lm("systemId %1. Error saving transaction %2 (%3, hash %4) to log. %5")
                .args(tran.systemId, ::ec2::ApiCommand::toString(tran.header.command),
                    tran.header, tran.hash, saveTranQuery.lastError().text()));
        return nx::sql::DBResult::ioError;
    }

    return nx::sql::DBResult::ok;
}

nx::sql::DBResult TransactionDataObject::updateTimestampHiForSystem(
    nx::sql::QueryContext* queryContext,
    const std::string& systemId,
    quint64 newValue)
{
    QSqlQuery saveSystemTimestampSequence(*queryContext->connection()->qtSqlConnection());
    saveSystemTimestampSequence.prepare(R"sql(
        REPLACE INTO transaction_source_settings(system_id, timestamp_hi) VALUES (?, ?)
        )sql");
    saveSystemTimestampSequence.addBindValue(systemId.c_str());
    saveSystemTimestampSequence.addBindValue(newValue);
    if (!saveSystemTimestampSequence.exec())
    {
        NX_WARNING(QnLog::EC2_TRAN_LOG.join(this),
            lm("systemId %1. Error saving transaction timestamp sequence %2 to log. %3")
                .args(systemId, newValue, saveSystemTimestampSequence.lastError().text()));
        return nx::sql::DBResult::ioError;
    }

    return nx::sql::DBResult::ok;
}

nx::sql::DBResult TransactionDataObject::fetchTransactionsOfAPeerQuery(
    nx::sql::QueryContext* queryContext,
    const std::string& systemId,
    const QString& peerId,
    const QString& dbInstanceId,
    std::int64_t minSequence,
    std::int64_t maxSequence,
    std::vector<dao::TransactionLogRecord>* const transactions)
{
    QSqlQuery fetchTransactionsOfAPeerQuery(*queryContext->connection()->qtSqlConnection());
    fetchTransactionsOfAPeerQuery.prepare(R"sql(
        SELECT tran_data, tran_hash, timestamp, sequence
        FROM transaction_log
        WHERE system_id=? AND peer_guid=? AND db_guid=? AND sequence>? AND sequence<=?
        ORDER BY sequence
    )sql");
    fetchTransactionsOfAPeerQuery.addBindValue(systemId.c_str());
    fetchTransactionsOfAPeerQuery.addBindValue(peerId);
    fetchTransactionsOfAPeerQuery.addBindValue(dbInstanceId);
    fetchTransactionsOfAPeerQuery.addBindValue((qint64)minSequence);
    fetchTransactionsOfAPeerQuery.addBindValue((qint64)maxSequence);
    if (!fetchTransactionsOfAPeerQuery.exec())
    {
        NX_ERROR(QnLog::EC2_TRAN_LOG.join(this),
            lm("systemId %1. Error executing fetch_transactions request for peer (%2; %3). %4")
                .args(peerId, dbInstanceId, fetchTransactionsOfAPeerQuery.lastError().text()));
        return nx::sql::DBResult::ioError;
    }

    while (fetchTransactionsOfAPeerQuery.next())
    {
        transactions->push_back(
            TransactionLogRecord{
                fetchTransactionsOfAPeerQuery.value("tran_hash").toByteArray(),
                std::make_unique<UbjsonTransactionPresentation>(
                    fetchTransactionsOfAPeerQuery.value("tran_data").toByteArray(),
                    m_transactionFormatVersion)
                });
    }

    return nx::sql::DBResult::ok;
}

} // namespace nx::clusterdb::engine::dao::rdb
