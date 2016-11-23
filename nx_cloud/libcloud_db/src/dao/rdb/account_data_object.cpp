#include "account_data_object.h"

#include <QtSql/QSqlQuery>
#include <QtSql/QSqlError>

#include <nx/fusion/model_functions.h>
#include <nx/utils/log/log.h>

namespace nx {
namespace cdb {
namespace dao {
namespace rdb {

nx::db::DBResult AccountDataObject::insert(
    nx::db::QueryContext* queryContext,
    const api::AccountData& account)
{
    QSqlQuery insertAccountQuery(*queryContext->connection());
    insertAccountQuery.prepare(
        R"sql(
        INSERT INTO account (id, email, password_ha1, password_ha1_sha256, full_name, customization, status_code)
        VALUES  (:id, :email, :passwordHa1, :passwordHa1Sha256, :fullName, :customization, :statusCode)
        )sql");
    QnSql::bind(account, &insertAccountQuery);
    if (!insertAccountQuery.exec())
    {
        NX_LOG(lm("Could not insert account (%1, %2) into DB. %3")
            .arg(account.email).arg(QnLexical::serialized(account.statusCode))
            .arg(insertAccountQuery.lastError().text()),
            cl_logDEBUG1);
        return nx::db::DBResult::ioError;
    }

    return nx::db::DBResult::ok;
}

} // namespace rdb
} // namespace dao
} // namespace cdb
} // namespace nx
