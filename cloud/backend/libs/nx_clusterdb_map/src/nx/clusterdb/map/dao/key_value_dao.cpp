#include "key_value_dao.h"

#include <QCryptographicHash>

#include <nx/sql/query.h>
#include <nx/sql/query_context.h>
#include <nx/fusion/serialization/sql.h>
#include <nx/sql/async_sql_query_executor.h>

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

std::string hash(const std::string& key)
{
    //SHA512 hash is 64 bytes. Converted to hex, we have 128 characters.
    return QCryptographicHash::hash(key.c_str(), QCryptographicHash::Sha512).toHex().constData();
}

} // namespace

KeyValueDao::KeyValueDao(const std::string& systemId):
    m_systemId(systemId.c_str())
{
}

void KeyValueDao::insertOrUpdate(
    nx::sql::QueryContext* queryContext,
    const std::string& key,
    const std::string& value)
{
    auto query = queryContext->connection()->createQuery();
    query->prepare(QString(kReplaceKeyValuePairTemplate).arg(m_systemId));
    query->bindValue(kKeyHashBinding, hash(key));
    query->bindValue(kKeyBinding, key);
    query->bindValue(kValueBinding, value);
    query->exec();
}

void KeyValueDao::remove(nx::sql::QueryContext* queryContext, const std::string& key)
{
    auto query = queryContext->connection()->createQuery();
    query->prepare(QString(kRemoveKeyValuePairTemplate).arg(m_systemId));
    query->bindValue(kKeyBinding, key);
    query->exec();
}

std::optional<std::string> KeyValueDao::get(
    nx::sql::QueryContext* queryContext,
    const std::string & key)
{
    auto query = queryContext->connection()->createQuery();
    query->prepare(QString(kFetchValueTemplate).arg(m_systemId));
    query->bindValue(kKeyBinding, key);
    query->exec();

    if (!query->next())
        return std::nullopt;

    return query->value(kValue).toString().toStdString();
}

std::map<std::string, std::string> KeyValueDao::getPairs(
    nx::sql::QueryContext* queryContext)
{
    auto query = queryContext->connection()->createQuery();
    query->prepare(QString(kFetchPairsTemplate).arg(m_systemId));
    query->exec();

    std::map<std::string, std::string> pairs;
    while (query->next())
    {
        pairs.emplace(
            query->value(kKey).toString().toStdString(),
            query->value(kValue).toString().toStdString());
    }

    return pairs;
}

std::optional<std::string> KeyValueDao::lowerBound(
    nx::sql::QueryContext* queryContext,
    const std::string& key)
{
    auto query = queryContext->connection()->createQuery();
    query->prepare(QString(kLowerBoundTemplate).arg(m_systemId));
    query->bindValue(kLowerBoundBinding, key);
    query->exec();

    if (!query->next())
        return std::nullopt;

    return query->value(kKey).toString().toStdString();
}

std::optional<std::string> KeyValueDao::upperBound(
    nx::sql::QueryContext* queryContext,
    const std::string& key)
{
    auto query = queryContext->connection()->createQuery();
    query->prepare(QString(kUpperBoundTemplate).arg(m_systemId));
    query->bindValue(kUpperBoundBinding, key);
    query->exec();

    if (!query->next())
        return std::nullopt;

    return query->value(kKey).toString().toStdString();
}

std::map<std::string, std::string> KeyValueDao::getRange(
    nx::sql::QueryContext* queryContext,
    const std::string& lowerBoundKey,
    const std::string& upperBoundKey)
{
    auto query = queryContext->connection()->createQuery();
    query->prepare(QString(kFetchRangeTemplate).arg(m_systemId));
    query->bindValue(kLowerBoundBinding, lowerBoundKey);
    query->bindValue(kUpperBoundBinding, upperBoundKey);
    query->exec();

    std::map<std::string, std::string> pairs;
    while (query->next())
    {
        pairs.emplace(
            query->value(kKey).toString().toStdString(),
            query->value(kValue).toString().toStdString());
    }

    return pairs;
}

} // namespace nx::clusterdb::map::dao
