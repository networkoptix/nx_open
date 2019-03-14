#include "key_value_dao.h"

#include <QCryptographicHash>

#include <nx/sql/query.h>
#include <nx/sql/query_context.h>
#include <nx/fusion/serialization/sql.h>
#include <nx/sql/async_sql_query_executor.h>

#include "schema_name.h"

namespace nx::clusterdb::map::dao {

namespace {

static constexpr char kKeyHashBinding[] = ":key_hash";
static constexpr char kKeyBinding[] = ":key";
static constexpr char kValueBinding[] = ":value";
static constexpr char kLowerBoundBinding[] = ":lower_bound";
static constexpr char kUpperBoundBinding[] = ":upper_bound";
static constexpr char kKey[] = "key";
static constexpr char kValue[] = "value";

// Note table name contains a guid with '-' character, requiring it to be escaped

static constexpr char kReplaceKeyValuePairTemplate[] = R"sql(

REPLACE INTO `%1_data`(key_hash, key, value)
VALUES(:key_hash, :key, :value)

)sql";


static constexpr char kRemoveKeyValuePairTemplate[] = R"sql(

DELETE FROM `%1_data` WHERE key=:key

)sql";

static constexpr char kFetchValueTemplate[] = R"sql(

SELECT value FROM `%1_data` where key=:key

)sql";

static constexpr char kFetchPairsTemplate[] = R"sql(

SELECT key, value from `%1_data`

)sql";

static constexpr char kLowerBoundTemplate[] = R"sql(

SELECT key FROM `%1_data` WHERE key >= :lower_bound

)sql";

static constexpr char kUpperBoundTemplate[] = R"sql(

SELECT key FROM `%1_data` WHERE key > :upper_bound

)sql";

static constexpr char kFetchRangeTemplate[] = R"sql(

SELECT key, value from `%1_data` WHERE key >= :lower_bound AND key <= :upper_bound

)sql";

static constexpr char kFetchRangeTemplateWithoutUpperBound[] = R"sql(

SELECT key, value from `%1_data` WHERE key >= :lower_bound

)sql";

std::string hash(const QByteArray& key)
{
    //SHA512 hash is 64 bytes. Converted to hex, we have 128 characters.
    return QCryptographicHash::hash(key, QCryptographicHash::Sha512).toHex().constData();
}

QByteArray toByteArray(const std::string& str)
{
    return QByteArray(str.c_str(), str.size());
}

std::map<std::string, std::string> toStdMap(nx::sql::AbstractSqlQuery* query)
{
    std::map<std::string, std::string> pairs;
    while (query->next())
    {
        pairs.emplace(
            query->value(kKey).toByteArray().toStdString(),
            query->value(kValue).toByteArray().toStdString());
    }
    return pairs;
}

} // namespace

void KeyValueDao::insertOrUpdate(
    nx::sql::QueryContext* queryContext,
    const std::string& key,
    const std::string& value)
{
    QByteArray keyBytes = toByteArray(key);
    auto query = queryContext->connection()->createQuery();
    query->prepare(QString(kReplaceKeyValuePairTemplate).arg(kSchemaName));
    query->bindValue(kKeyHashBinding, hash(keyBytes));
    query->bindValue(kKeyBinding, keyBytes);
    query->bindValue(kValueBinding, toByteArray(value));
    query->exec();
}

void KeyValueDao::remove(nx::sql::QueryContext* queryContext, const std::string& key)
{
    auto query = queryContext->connection()->createQuery();
    query->prepare(QString(kRemoveKeyValuePairTemplate).arg(kSchemaName));
    query->bindValue(kKeyBinding, toByteArray(key));
    query->exec();
}

std::optional<std::string> KeyValueDao::get(
    nx::sql::QueryContext* queryContext,
    const std::string& key)
{
    auto query = queryContext->connection()->createQuery();
    query->prepare(QString(kFetchValueTemplate).arg(kSchemaName));
    query->bindValue(kKeyBinding, toByteArray(key));
    query->exec();

    if (!query->next())
        return std::nullopt;

    return query->value(kValue).toByteArray().toStdString();
}

std::map<std::string, std::string> KeyValueDao::getPairs(
    nx::sql::QueryContext* queryContext)
{
    auto query = queryContext->connection()->createQuery();
    query->prepare(QString(kFetchPairsTemplate).arg(kSchemaName));
    query->exec();

    return toStdMap(query.get());
}

std::optional<std::string> KeyValueDao::lowerBound(
    nx::sql::QueryContext* queryContext,
    const std::string& key)
{
    auto query = queryContext->connection()->createQuery();
    query->prepare(QString(kLowerBoundTemplate).arg(kSchemaName));
    query->bindValue(kLowerBoundBinding, toByteArray(key));
    query->exec();

    if (!query->next())
        return std::nullopt;

    return query->value(kKey).toByteArray().toStdString();
}

std::optional<std::string> KeyValueDao::upperBound(
    nx::sql::QueryContext* queryContext,
    const std::string& key)
{
    auto query = queryContext->connection()->createQuery();
    query->prepare(QString(kUpperBoundTemplate).arg(kSchemaName));
    query->bindValue(kUpperBoundBinding, toByteArray(key));
    query->exec();

    if (!query->next())
        return std::nullopt;

    return query->value(kKey).toByteArray().toStdString();
}

std::map<std::string, std::string> KeyValueDao::getRange(
    nx::sql::QueryContext* queryContext,
    const std::string& keyLowerBound,
    const std::string& keyUpperBound)
{
    auto query = queryContext->connection()->createQuery();
    query->prepare(QString(kFetchRangeTemplate).arg(kSchemaName));
    query->bindValue(kLowerBoundBinding, toByteArray(keyLowerBound));
    query->bindValue(kUpperBoundBinding, toByteArray(keyUpperBound));
    query->exec();

    return toStdMap(query.get());
}

std::map<std::string, std::string> KeyValueDao::getRange(
    nx::sql::QueryContext* queryContext,
    const std::string& keyLowerBound)
{
    auto query = queryContext->connection()->createQuery();
    query->prepare(QString(kFetchRangeTemplateWithoutUpperBound).arg(kSchemaName));
    query->bindValue(kLowerBoundBinding, toByteArray(keyLowerBound));
    query->exec();

    return toStdMap(query.get());
}

} // namespace nx::clusterdb::map::dao
