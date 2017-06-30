#include "cassandra_connection.h"

using namespace nx::utils;

namespace {
using namespace nx::cassandra;

struct Context
{
    nx::cassandra::Connection* ctx;
    MoveOnlyFunc<void(CassError)> cb;

    Context(nx::cassandra::Connection* ctx, MoveOnlyFunc<void(CassError)> cb):
        ctx(ctx),
        cb(std::move(cb))
    {}

    void free()
    {
        delete this;
    }
};

void* makeContext(Connection* connection, MoveOnlyFunc<void(CassError)> cb)
{
    return new Context(connection, std::move(cb));
}

static void connectionCb(CassFuture* future, void* data)
{
    CassError result = cass_future_error_code(future);
    Context* ctx = (Context*)data;
    ctx->cb(result);
    ctx->free();
}

} // namespace

namespace nx {
namespace cassandra {

Connection::Connection(const char* host):
    m_cluster(cass_cluster_new()),
    m_session(cass_session_new())
{
    cass_cluster_set_contact_points(m_cluster, host);
}

void Connection::initAsync(nx::utils::MoveOnlyFunc<void(CassError)> initCb)
{
    CassFuture* future = cass_session_connect(m_session, m_cluster);
    cass_future_set_callback(future, &connectionCb, makeContext(this, std::move(initCb)));
    cass_future_free(future);
}

CassError Connection::initSync()
{
    CassFuture* future = cass_session_connect(m_session, m_cluster);
    auto result = cass_future_error_code(future);
    cass_future_free(future);

    return result;
}

void Connection::queryAsync(
    const Query& /*query*/,
    nx::utils::MoveOnlyFunc<void(CassError, const QueryResult&)> /*queryCb*/)
{

}

boost::optional<QueryResult> Connection::querySync(const Query& /*query*/)
{
    return boost::none;
}

Connection::~Connection()
{
    cass_session_free(m_session);
    cass_cluster_free(m_cluster);
}

} // namespace cassandra
} // namespace nx
