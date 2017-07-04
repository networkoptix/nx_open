#pragma once

#include <string>
#include <cassandra.h>
#include <nx/utils/move_only_func.h>
#include <nx/utils/thread/cf/cfuture.h>
#include <nx/utils/thread/mutex.h>

namespace nx {
namespace cassandra {

class Query
{
public:
    Query(CassFuture* future);
    Query() = default;
    ~Query();

    Query(const Query&) = delete;
    Query& operator=(const Query&) = delete;

    Query(Query&&);
    Query& operator=(Query&&);


    bool bind(const std::string& key, const std::string& value);
    bool bind(const std::string& key, bool value);
    bool bind(const std::string& key, double value);
    bool bind(const std::string& key, float value);
    bool bind(const std::string& key, int32_t value);
    bool bind(const std::string& key, int64_t value);

private:
    const CassPrepared* m_prepared = nullptr;
    CassStatement* m_statement = nullptr;

    friend class AsyncConnection;
};

class QueryResult
{
public:
    QueryResult(CassFuture* future);
    QueryResult() = default;
    ~QueryResult();

    QueryResult(const QueryResult&) = delete;
    QueryResult& operator=(const QueryResult&) = delete;

    QueryResult(QueryResult&&);
    QueryResult& operator=(QueryResult&&);

    bool next();
    bool get(const std::string& key, std::string* value) const;
    bool get(const std::string& key, bool* value) const;
    bool get(const std::string& key, double* value) const;
    bool get(const std::string& key, float* value) const;
    bool get(const std::string& key, int32_t* value) const;
    bool get(const std::string& key, int64_t* value) const;


private:
    const CassResult* m_result = nullptr;
    CassIterator* m_iterator = nullptr;

    CassRow* nextRow() const;
};

class AsyncConnection
{
public:
    AsyncConnection(const char* host);
    ~AsyncConnection();

    AsyncConnection(const AsyncConnection&) = delete;
    AsyncConnection& operator=(const AsyncConnection&) = delete;

    AsyncConnection(AsyncConnection&&) = default;
    AsyncConnection& operator=(AsyncConnection&&) = default;

    void init(nx::utils::MoveOnlyFunc<void(CassError)> initCb);

    void prepareQuery(
        const char* queryString,
        nx::utils::MoveOnlyFunc<void(CassError, Query query)> prepareCb);

    void executeSelect(
        Query query,
        nx::utils::MoveOnlyFunc<void(CassError, QueryResult result)> selectCb);

    void executeUpdate(
        Query query,
        nx::utils::MoveOnlyFunc<void(CassError)> updateCb);

    cf::future<CassError> init();
    cf::future<std::pair<CassError, Query>> prepareQuery(const char* queryString);
    cf::future<std::pair<CassError, QueryResult>> executeSelect(Query query);
    cf::future<CassError> executeUpdate(Query query);

private:
    CassCluster* m_cluster = nullptr;
    CassSession* m_session = nullptr;
    QnMutex m_mutex;
};

} // namespace cassandra
} // namespace nx
