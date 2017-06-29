#include <cassandra.h>
#include <nx/utils/move_only_func.h>
#include <nx/utils/thread/mutex.h>

namespace nx {
namespace cassandra {

class AsyncConnection
{
public:
    AsyncConnection(const char* host);
    ~AsyncConnection();

    void init(nx::utils::MoveOnlyFunc<void(CassError)> initCb);

private:
    CassCluster* m_cluster = nullptr;
    CassSession* m_session = nullptr;
    QnMutex m_mutex;

    friend void connectionCb(CassFuture* future, void* data);
};

} // namespace cassandra
} // namespace nx
