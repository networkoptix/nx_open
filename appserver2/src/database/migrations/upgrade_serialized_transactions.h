#pragma once

#include <QtSql/QSqlDatabase>
#include <QtSql/QSqlError>
#include <QtSql/QSqlQuery>

#include <nx/fusion/model_functions.h>
#include <nx/fusion/serialization/sql.h>

namespace ec2 {
namespace migration {

template<
    typename OldTransactionType,
    typename NewTransactionType
>
bool upgradeSerializedTransactions(QSqlDatabase* const sdb)
{
    // migrate transaction log
    QSqlQuery query(*sdb);
    query.setForwardOnly(true);
    query.prepare(
        R"sql(
        SELECT tran_guid, tran_data from transaction_log
        )sql");
    if (!query.exec())
    {
        qWarning() << Q_FUNC_INFO << query.lastError().text();
        return false;
    }

    QSqlQuery updQuery(*sdb);
    updQuery.prepare(
        R"sql(
        UPDATE transaction_log SET tran_data = ?, tran_type = ? WHERE tran_guid = ?
        )sql");

    while (query.next())
    {
        QnUuid tranGuid = QnSql::deserialized_field<QnUuid>(query.value(0));
        QByteArray srcData = query.value(1).toByteArray();
        QnUbjsonReader<QByteArray> stream(&srcData);

        OldTransactionType oldTransaction;
        const int srcDataSize = QnUbjson::serialized(oldTransaction).size();
        if (!QnUbjson::deserialize(&stream, &oldTransaction))
        {
            qWarning() << Q_FUNC_INFO << "Can' deserialize transaction from transaction log";
            return false;
        }

        NewTransactionType newTransaction;
        // Actual migration is done here.
        newTransaction = oldTransaction;

        QByteArray dstData = QnUbjson::serialized(newTransaction);
        dstData.append(srcData.mid(srcDataSize));

        updQuery.addBindValue(dstData);
        updQuery.addBindValue(newTransaction.transactionType);
        updQuery.addBindValue(QnSql::serialized_field(tranGuid));
        if (!updQuery.exec())
        {
            qWarning() << Q_FUNC_INFO << query.lastError().text();
            return false;
        }
    }

    return true;
}

} // namespace migration
} // namespace ec2
