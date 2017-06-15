#include "transaction_data_object.h"

#include <QtSql/QSqlError>
#include <QtSql/QSqlQuery>

#include <nx/utils/log/log.h>

namespace nx {
namespace cdb {
namespace ec2 {
namespace dao {
namespace rdb {

nx::db::DBResult TransactionDataObject::insertOrReplaceTransaction(
    nx::db::QueryContext* queryContext,
    const TransactionData& tran)
{
    QSqlQuery saveTranQuery(*queryContext->connection());
    saveTranQuery.prepare(
        R"sql(
        REPLACE INTO transaction_log(system_id, peer_guid, db_guid, sequence,
                                     timestamp_hi, timestamp, tran_hash, tran_data)
        VALUES (?, ?, ?, ?, ?, ?, ?, ?)
        )sql");
    saveTranQuery.addBindValue(QLatin1String(tran.systemId));
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

        NX_LOGX(QnLog::EC2_TRAN_LOG,
            lm("systemId %1. Error saving transaction %2 (%3, hash %4) to log. %5")
            .arg(tran.systemId).arg(::ec2::ApiCommand::toString(tran.header.command))
            .arg(tran.header).arg(tran.hash).arg(saveTranQuery.lastError().text()),
            cl_logWARNING);
        return nx::db::DBResult::ioError;
    }

    return nx::db::DBResult::ok;
}

nx::db::DBResult TransactionDataObject::updateTimestampHiForSystem(
    nx::db::QueryContext* queryContext,
    const nx::String& systemId,
    quint64 newValue)
{
    QSqlQuery saveSystemTimestampSequence(*queryContext->connection());
    saveSystemTimestampSequence.prepare(
        R"sql(
        REPLACE INTO transaction_source_settings(system_id, timestamp_hi) VALUES (?, ?)
        )sql");
    saveSystemTimestampSequence.addBindValue(QLatin1String(systemId));
    saveSystemTimestampSequence.addBindValue(newValue);
    if (!saveSystemTimestampSequence.exec())
    {
        NX_LOGX(QnLog::EC2_TRAN_LOG,
            lm("systemId %1. Error saving transaction timestamp sequence %2 to log. %3")
            .arg(systemId).arg(newValue).arg(saveSystemTimestampSequence.lastError().text()),
            cl_logWARNING);
        return nx::db::DBResult::ioError;
    }

    return nx::db::DBResult::ok;
}

nx::db::DBResult TransactionDataObject::fetchTransactionsOfAPeerQuery(
    nx::db::QueryContext* queryContext,
    const nx::String& systemId,
    const QString& peerId,
    const QString& dbInstanceId,
    std::int64_t minSequence,
    std::int64_t maxSequence,
    std::vector<dao::TransactionLogRecord>* const transactions)
{
    QSqlQuery fetchTransactionsOfAPeerQuery(*queryContext->connection());
    fetchTransactionsOfAPeerQuery.prepare(
        R"sql(
            SELECT tran_data, tran_hash, timestamp, sequence
            FROM transaction_log
            WHERE system_id=? AND peer_guid=? AND db_guid=? AND sequence>? AND sequence<=?
            ORDER BY sequence
        )sql");
    fetchTransactionsOfAPeerQuery.addBindValue(QLatin1String(systemId));
    fetchTransactionsOfAPeerQuery.addBindValue(peerId);
    fetchTransactionsOfAPeerQuery.addBindValue(dbInstanceId);
    fetchTransactionsOfAPeerQuery.addBindValue((qint64)minSequence);
    fetchTransactionsOfAPeerQuery.addBindValue((qint64)maxSequence);
    if (!fetchTransactionsOfAPeerQuery.exec())
    {
        NX_LOGX(QnLog::EC2_TRAN_LOG,
            lm("systemId %1. Error executing fetch_transactions request "
                "for peer (%2; %3). %4")
            .arg(peerId).arg(dbInstanceId).arg(fetchTransactionsOfAPeerQuery.lastError().text()),
            cl_logERROR);
        return nx::db::DBResult::ioError;
    }

    while (fetchTransactionsOfAPeerQuery.next())
    {
        transactions->push_back(
            TransactionLogRecord{
                fetchTransactionsOfAPeerQuery.value("tran_hash").toByteArray(),
                std::make_unique<UbjsonTransactionPresentation>(
                    fetchTransactionsOfAPeerQuery.value("tran_data").toByteArray(),
                    nx_ec::EC2_PROTO_VERSION)
                });
    }

    return nx::db::DBResult::ok;
}

} // namespace rdb
} // namespace dao
} // namespace ec2
} // namespace cdb
} // namespace nx
