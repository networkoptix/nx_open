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
        NX_LOGX(QnLog::EC2_TRAN_LOG,
            lm("systemId %1. Error saving transaction %2 (%3, hash %4) to log. %5")
            .arg(tran.systemId).arg(::ec2::ApiCommand::toString(tran.header.command))
            .str(tran.header).arg(tran.hash).arg(saveTranQuery.lastError().text()),
            cl_logWARNING);
        return nx::db::DBResult::ioError;
    }

    return nx::db::DBResult::ok;
}

} // namespace rdb
} // namespace dao
} // namespace ec2
} // namespace cdb
} // namespace nx
