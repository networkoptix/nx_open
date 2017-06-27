#pragma once

#include <QtSql/QSqlDatabase>
#include <QtSql/QSqlError>
#include <QtSql/QSqlQuery>

#include <nx/fusion/model_functions.h>
#include <nx/fusion/serialization/sql.h>
#include <nx/utils/log/log.h>

namespace nx {
namespace cdb {
namespace ec2 {
namespace migration {
namespace detail {

template<typename TransactionHeaderType>
class Transaction
{
    typedef Transaction<TransactionHeaderType> SelfType;

public:
    QByteArray systemId;
    QByteArray hash;
    TransactionHeaderType header;
    QByteArray data;

    Transaction()
    {
    }

    template<typename OtherTransactionHeaderType>
    Transaction(const Transaction<OtherTransactionHeaderType>& right)
    {
        systemId = right.systemId;
        hash = right.hash;
        header = right.header;
        data = right.data;
    }

    bool readFrom(const QSqlRecord& record)
    {
        systemId = QnSql::deserialized_field<QByteArray>(record.value("system_id"));
        hash = QnSql::deserialized_field<QByteArray>(record.value("tran_hash"));
        const auto serializedOldTransaction =
            QnSql::deserialized_field<QByteArray>(record.value("tran_data"));

        QnUbjsonReader<QByteArray> stream(&serializedOldTransaction);
        if (!QnUbjson::deserialize(&stream, &header))
            return false;

        data = serializedOldTransaction.mid(stream.pos());
        return true;
    }

    void writeTo(QSqlQuery* const query) const
    {
        query->bindValue(":tran_data", QnUbjson::serialized(header) + data);
        query->bindValue(":system_id", systemId);
        query->bindValue(":tran_hash", hash);
    }
};

template<
    typename OldTransactionType,
    typename NewTransactionType
>
nx::utils::db::DBResult upgradeSerializedTransactions(
    nx::utils::db::QueryContext* const queryContext)
{
    QSqlQuery fetchCurrentTransactions(*queryContext->connection());
    fetchCurrentTransactions.setForwardOnly(true);
    fetchCurrentTransactions.prepare(
        R"sql(
        SELECT system_id, tran_hash, tran_data FROM transaction_log
        )sql");
    if (!fetchCurrentTransactions.exec())
    {
        NX_LOG(lm("Error fetching transactions. %1")
            .arg(fetchCurrentTransactions.lastError().text()),
            cl_logWARNING);
        return nx::utils::db::DBResult::ioError;
    }

    QSqlQuery saveUpdatedTransactionQuery(*queryContext->connection());
    saveUpdatedTransactionQuery.prepare(
        R"sql(
        UPDATE transaction_log SET tran_data = :tran_data
        WHERE system_id = :system_id AND tran_hash = :tran_hash
        )sql");

    while (fetchCurrentTransactions.next())
    {
        Transaction<OldTransactionType> oldTransaction;
        if (!oldTransaction.readFrom(fetchCurrentTransactions.record()))
        {
            NX_CRITICAL(false);
            return nx::utils::db::DBResult::ioError;
        }

        Transaction<NewTransactionType> newTransaction = oldTransaction;
        newTransaction.writeTo(&saveUpdatedTransactionQuery);
        if (!saveUpdatedTransactionQuery.exec())
        {
            NX_LOG(lm("Error saving new transaction to DB. %1")
                .arg(saveUpdatedTransactionQuery.lastError().text()),
                cl_logWARNING);
            return nx::utils::db::DBResult::ioError;
        }
    }

    return nx::utils::db::DBResult::ok;
}

} // namespace detail
} // namespace migration
} // namespace ec2
} // namespace cdb
} // namespace nx
