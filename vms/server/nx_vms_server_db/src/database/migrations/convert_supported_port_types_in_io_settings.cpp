#include "convert_supported_port_types_in_io_settings.h"

#include <QByteArray>
#include <QtSql/QSqlDatabase>
#include <QtSql/QSqlQuery>

#include <nx/fusion/serialization/json_functions.h>
#include <nx/fusion/serialization/json.h>
#include <nx/sql/sql_query_execution_helper.h>

static bool convertSupportedPortTypes(QByteArray* data)
{
    std::vector<QJsonObject> list;
    if (!QJson::deserialize(*data, &list))
        return false;
    const QString kField = "supportedPortTypes";
    const QString kPrefixToRemove = "PT_";
    bool converted = false;
    for (auto& item: list)
    {
        auto it = item.find(kField);
        if (it == item.end())
            continue;
        if (!it->isString())
            continue;
        QString value = it->toString();
        if (value.isEmpty())
            continue;
        value.replace(kPrefixToRemove, QString());
        it.value() = value;
        converted = true;
    }
    if (!converted)
        return false;
    *data = QJson::serialized(list);
    return true;
}

namespace ec2::detail {

bool convertSupportedPortTypesInIoSettings(const QSqlDatabase& database, bool* converted)
{
    using namespace nx::sql;

    *converted = false;
    QSqlQuery query(database);
    query.setForwardOnly(true);
    const QString kQuery = "SELECT rowid, value FROM vms_kvpair WHERE name = 'ioSettings'";

    if (!SqlQueryExecutionHelper::prepareSQLQuery(&query, kQuery, Q_FUNC_INFO))
        return false;
    if (!SqlQueryExecutionHelper::execSQLQuery(&query, Q_FUNC_INFO))
        return false;

    QSqlQuery insertQuery(database);
    const QString kInsertQuery = "UPDATE vms_kvpair SET value = :value WHERE rowid = :rowid";

    while (query.next())
    {
        const int rowid = query.value(0).toInt();
        QByteArray value = query.value(1).toByteArray();
        if (!convertSupportedPortTypes(&value))
            continue;

        if (!SqlQueryExecutionHelper::prepareSQLQuery(&insertQuery, kInsertQuery, Q_FUNC_INFO))
            return false;
        insertQuery.bindValue(":value", value);
        insertQuery.bindValue(":rowid", rowid);
        if (!SqlQueryExecutionHelper::execSQLQuery(&insertQuery, Q_FUNC_INFO))
            return false;

        *converted = true;
    }

    return true;
}

} // namespace ec2::detail
