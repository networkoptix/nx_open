/**********************************************************
* Jun 1, 2016
* a.kolesnikov
***********************************************************/

#include "request_executor.h"

#include <QtSql/QSqlError>


namespace nx {
namespace db {

DBResult BaseExecutor::detailResultCode(
    QSqlDatabase* const connection,
    DBResult result) const
{
    if (result != DBResult::ioError)
        return result;
    switch (connection->lastError().type())
    {
        case QSqlError::StatementError:
            return DBResult::statementError;
        default:
            return result;
    }
}

DBResult BaseExecutor::lastDBError(QSqlDatabase* const connection) const
{
    switch (connection->lastError().type())
    {
        case QSqlError::StatementError:
            return DBResult::statementError;

        case QSqlError::NoError:    //Qt not always sets error code correctly
        case QSqlError::ConnectionError:
        case QSqlError::TransactionError:
        case QSqlError::UnknownError:
        default:
            return DBResult::ioError;
    }
}

}   //namespace db
}   //namespace nx
