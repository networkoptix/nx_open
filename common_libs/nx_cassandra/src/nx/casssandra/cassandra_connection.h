#include <string>
#include <cassandra.h>
#include <nx/utils/move_only_func.h>
#include <nx/utils/thread/mutex.h>

namespace nx {
namespace cassandra {

class Query
{
public:
    bool bind(const std::string& key, const std::string& value);
    bool bind(const std::string& key, bool value);
    bool bind(const std::string& key, double value);
    bool bind(const std::string& key, float value);
    bool bind(const std::string& key, int32_t value);
    bool bind(const std::string& key, int64_t value);

private:
    CassPrepared* m_prepared = nullptr;
    CassStatement* m_statement = nullptr;

    Query(CassFuture* future);

    Query(const Query&) = delete;
    Query& operator=(const Query&) = delete;

    Query(Query&&) = default;
    Query& operator=(Query&&) = default;

    friend void onPrepare(CassFuture* future, void* data);
    friend class Connection;
};

class QueryResult
{
public:
private:
};

class Connection
{
public:
    Connection(const char* host);
    ~Connection();

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
        nx::utils::MoveOnlyFunc<void(CassError)> executeCb);

private:
    CassCluster* m_cluster = nullptr;
    CassSession* m_session = nullptr;
    QnMutex m_mutex;

    friend void onConnect(CassFuture* future, void* data);
};

} // namespace cassandra
} // namespace nx
