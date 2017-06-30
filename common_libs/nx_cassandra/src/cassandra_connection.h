#include <cassandra.h>
#include <nx/utils/move_only_func.h>
#include <nx/utils/thread/mutex.h>

namespace nx {
namespace cassandra {

class Query
{
public:
private:
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

    void queryAsync(
        const Query& query,
        nx::utils::MoveOnlyFunc<void(CassError, const QueryResult&)> queryCb);

    boost::optional<QueryResult> querySync(const Query& query);

private:
    CassCluster* m_cluster = nullptr;
    CassSession* m_session = nullptr;
    QnMutex m_mutex;

    friend void connectionCb(CassFuture* future, void* data);
};

} // namespace cassandra
} // namespace nx
