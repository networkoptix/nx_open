#include <string>
#include <cassandra.h>
#include <nx/utils/move_only_func.h>
#include <nx/utils/thread/mutex.h>

namespace nx {
namespace cassandra {

class Query
{
public:
    Query(CassFuture* future);
    ~Query();

    Query(const Query&) = delete;
    Query& operator=(const Query&) = delete;

    Query(Query&&) = default;
    Query& operator=(Query&&) = default;


    bool bind(const std::string& key, const std::string& value);
    bool bind(const std::string& key, bool value);
    bool bind(const std::string& key, double value);
    bool bind(const std::string& key, float value);
    bool bind(const std::string& key, int32_t value);
    bool bind(const std::string& key, int64_t value);

private:
    const CassPrepared* m_prepared = nullptr;
    CassStatement* m_statement = nullptr;

    friend class Connection;
};

class QueryResult
{
public:
    QueryResult(CassFuture* future);
    ~QueryResult();

    QueryResult(const QueryResult&) = delete;
    QueryResult& operator=(const QueryResult&) = delete;

    QueryResult(QueryResult&&) = default;
    QueryResult& operator=(QueryResult&&) = default;

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

class Connection
{
public:
    Connection(const char* host);
    ~Connection();

    Connection(const Connection&) = delete;
    Connection& operator=(const Connection&) = delete;

    Connection(Connection&&) = default;
    Connection& operator=(Connection&&) = default;

    void initAsync(nx::utils::MoveOnlyFunc<void(CassError)> initCb);
    CassError initSync();

    void prepareQueryAsync(
        const char* queryString,
        nx::utils::MoveOnlyFunc<void(CassError, Query query)> prepareCb);

    void executeSelectAsync(
        Query query,
        nx::utils::MoveOnlyFunc<void(CassError, const QueryResult& result)> selectCb);

    void executeUpdateAsync(
        Query query,
        nx::utils::MoveOnlyFunc<void(CassError)> updateCb);

private:
    CassCluster* m_cluster = nullptr;
    CassSession* m_session = nullptr;
    QnMutex m_mutex;

    friend void onConnect(CassFuture* future, void* data);
};

} // namespace cassandra
} // namespace nx
