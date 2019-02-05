#include "key_value_dao.h"

#include <QcryptographicHash>

#include <nx/sql/query.h>
#include <nx/sql/query_context.h>
#include <nx/fusion/serialization/sql.h>
#include <nx/sql/async_sql_query_executor.h>

namespace nx::clusterdb::map::dao {

namespace {

static constexpr char kKeyHashBinding[] = ":key_hash";
static constexpr char kKeyBinding[] = ":key";
static constexpr char kValueBinding[] = ":value";
static constexpr char kKey[] = "key";
static constexpr char kValue[] = "value";

static constexpr char kReplaceKeyValuePair[] = R"sql(

REPLACE INTO data(key_hash, key, value)
VALUES(:key_hash, :key, :value)

)sql";


static constexpr char kRemoveKeyValuePair[] = R"sql(

DELETE FROM data WHERE key=:key

)sql";


static constexpr char kFetchKeyValuePairs[] = R"sql(

SELECT key, value FROM data

)sql";

static constexpr char kfetchValue[] = R"sql(

SELECT value FROM data where key=:key

)sql";


std::string hash(const std::string& key)
{
    //SHA512 hash is 64 bytes. Converted to hex, we have 128 characters.
    return QCryptographicHash::hash(key.c_str(), QCryptographicHash::Sha512).toHex().constData();
}

} // namespace

KeyValueDao::KeyValueDao(nx::sql::AsyncSqlQueryExecutor* queryExecutor)
    :
    m_queryExecutor(queryExecutor)
{
}

void KeyValueDao::save(
    nx::sql::QueryContext* queryContext,
    const std::string& key,
    const std::string& value)
{
    auto query = queryContext->connection()->createQuery();
    query->prepare(kReplaceKeyValuePair);
    query->bindValue(kKeyHashBinding, hash(key));
    query->bindValue(kKeyBinding, key);
    query->bindValue(kValueBinding, value);
    query->exec();
}

void KeyValueDao::remove(nx::sql::QueryContext* queryContext, const std::string& key)
{
    auto query = queryContext->connection()->createQuery();
    query->prepare(kRemoveKeyValuePair);
    query->bindValue(kKeyBinding, key);
    query->exec();
}

std::optional<std::string> KeyValueDao::get(
    nx::sql::QueryContext* queryContext,
    const std::string & key)
{
    auto query = queryContext->connection()->createQuery();
    query->prepare(kfetchValue);
    query->bindValue(kKeyBinding, key);
    query->exec();

    if (!query->next())
        return std::nullopt;

    return query->value(kValue).toString().toStdString();
}

nx::sql::AsyncSqlQueryExecutor& KeyValueDao::queryExecutor()
{
    return *m_queryExecutor;
}

} // namespace nx::clusterdb::map::dao::rdb
