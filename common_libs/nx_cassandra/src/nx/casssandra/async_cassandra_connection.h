#pragma once

#include <string>
#include <cassandra.h>
#include <nx/utils/move_only_func.h>
#include <nx/utils/thread/cf/cfuture.h>
#include <nx/utils/thread/mutex.h>

namespace nx {
namespace cassandra {

class NX_CASSANDRA_API Query
{
public:
    Query(CassFuture* future);
    Query(CassStatement* statement);
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

class NX_CASSANDRA_API QueryResult
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

    bool get(int index, std::string* value) const;
    bool get(int index, bool* value) const;
    bool get(int index, double* value) const;
    bool get(int index, float* value) const;
    bool get(int index, int32_t* value) const;
    bool get(int index, int64_t* value) const;

private:
    const CassResult* m_result = nullptr;
    CassIterator* m_iterator = nullptr;

    CassRow* nextRow() const;

    template<typename GetIndexFunc>
    bool get(GetIndexFunc getIndexFunc, std::string* value) const
    {
        if (!m_iterator)
            return false;

        const CassRow* row = cass_iterator_get_row(m_iterator);
        if (!row)
            return false;

        const char* val;
        size_t valSize;
        auto result = cass_value_get_string(getIndexFunc(row), &val, &valSize);

        if (result == CASS_OK)
        {
            value->resize(valSize);
            memcpy((void*)value->data(), val, valSize);
        }

        return result == CASS_OK;
    }
};

class NX_CASSANDRA_API AsyncConnection
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

    void executeSelect(
        const char* queryString,
        nx::utils::MoveOnlyFunc<void(CassError, QueryResult result)> selectCb);

    void executeUpdate(
        Query query,
        nx::utils::MoveOnlyFunc<void(CassError)> updateCb);

    void executeUpdate(
        const char* queryString,
        nx::utils::MoveOnlyFunc<void(CassError)> updateCb);

    cf::future<CassError> init();
    cf::future<std::pair<CassError, Query>> prepareQuery(const char* queryString);

    cf::future<std::pair<CassError, QueryResult>> executeSelect(Query query);
    cf::future<std::pair<CassError, QueryResult>> executeSelect(const char* queryString);

    cf::future<CassError> executeUpdate(Query query);
    cf::future<CassError> executeUpdate(const char* queryString);

private:
    CassCluster* m_cluster = nullptr;
    CassSession* m_session = nullptr;
    QnMutex m_mutex;
};

} // namespace cassandra
} // namespace nx
