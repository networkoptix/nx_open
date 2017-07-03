#include "cassandra_connection.h"

using namespace nx::utils;

namespace {
using namespace nx::cassandra;

struct ErrorCbConnectionContext
{
    nx::cassandra::Connection* ctx;
    MoveOnlyFunc<void(CassError)> cb;

    ErrorCbConnectionContext(nx::cassandra::Connection* ctx, MoveOnlyFunc<void(CassError)> cb):
        ctx(ctx),
        cb(std::move(cb))
    {}

    void free()
    {
        delete this;
    }
};

void* makeErrorCbConnectionContext(Connection* connection, MoveOnlyFunc<void(CassError)> cb)
{
    return new ErrorCbConnectionContext(connection, std::move(cb));
}

static void onDone(CassFuture* future, void* data)
{
    CassError result = cass_future_error_code(future);
    ErrorCbConnectionContext* ctx = (Context*)data;
    ctx->cb(result);
    ctx->free();
}


struct QueryCbConnectionContext
{
    nx::cassandra::Connection* ctx;
    MoveOnlyFunc<void(CassError, Query query)> cb;

    QueryCbConnectionContext(
        nx::cassandra::Connection* ctx,
        MoveOnlyFunc<void(CassError, Query query)> cb)
        :
        ctx(ctx),
        cb(std::move(cb))
    {}

    void free()
    {
        delete this;
    }
};

void* makeQueryCbConnectionContext(
    Connection* connection,
    MoveOnlyFunc<void(CassError, Query query)> cb)
{
    return new QueryCbConnectionContext(connection, std::move(cb));
}

static void onPrepare(CassFuture* future, void* data)
{
    CassError result = cass_future_error_code(future);
    QueryCbConnectionContext* ctx = (Context*)data;
    ctx->cb(result, Query(future));
    ctx->free();
}


struct SelectCbConnectionContext
{
    nx::cassandra::Connection* ctx;
    Query query;
    MoveOnlyFunc<void(CassError, const QueryResult&)> cb;

    QueryCbConnectionContext(
        nx::cassandra::Connection* ctx,
        Query query,
        MoveOnlyFunc<void(CassError, const QueryResult&)> cb)
        :
        ctx(ctx),
        query(std::move(query)),
        cb(std::move(cb))
    {}

    void free()
    {
        delete this;
    }
};

void* makeSelectCbConnectionContext(
    Connection* connection,
    Query query,
    MoveOnlyFunc<void(CassError, const QueryResult&)> cb)
{
    return new SelectCbConnectionContext(connection, std::move(query), std::move(cb));
}

static void onSelect(CassFuture* future, void* data)
{
    CassError result = cass_future_error_code(future);
    ExecuteCbConnectionContext* ctx = (Context*)data;
    ctx->cb(result, QueryResult(future));
    ctx->free();
}

} // namespace

namespace nx {
namespace cassandra {

/** ---------------------------------- Query ----------------------------------------------------*/

Query::Query(CassFuture* future)
{
    if (cass_future_error_code(future) != CASS_OK)
        return;

    m_prepared = cass_future_get_prepared(future);
    if (!m_prepared)
        return;

    m_statement = cass_prepared_bind(m_prepared);
}

bool Query::bind(const std::string& key, const std::string& value)
{
    if (!m_statement)
        return false;

    auto result = cass_statement_bind_string_by_name_n(
        m_statement,
        key.data(), key.size(),
        value.data(), value.size());

    return result == CASS_OK;
}

bool Query::bind(const std::string& key, bool value)
{
    if (!m_statement)
        return false;

    auto result = cass_statement_bind_bool_by_name_n(
        m_statement,
        key.data(), key.size(),
        (cass_bool_t)value);

    return result == CASS_OK;
}

bool Query::bind(const std::string& key, double value)
{
    if (!m_statement)
        return false;

    auto result = cass_statement_bind_double_by_name_n(
        m_statement,
        key.data(), key.size(),
        (cass_double_t)value);

    return result == CASS_OK;
}

bool Query::bind(const std::string& key, float value)
{
    if (!m_statement)
        return false;

    auto result = cass_statement_bind_float_by_name_n(
        m_statement,
        key.data(), key.size(),
        (cass_float_t)value);

    return result == CASS_OK;
}

bool Query::bind(const std::string& key, int32_t value)
{
    if (!m_statement)
        return false;

    auto result = cass_statement_bind_int32_by_name_n(
        m_statement,
        key.data(), key.size(),
        (cass_int32_t)value);

    return result == CASS_OK;
}

bool Query::bind(const std::string& key, int64_t value)
{
    if (!m_statement)
        return false;

    auto result = cass_statement_bind_int64_by_name_n(
        m_statement,
        key.data(), key.size(),
        (cass_int64_t)value);

    return result == CASS_OK;
}

/** ------------------------------- Connection --------------------------------------------------*/

Connection::Connection(const char* host):
    m_cluster(cass_cluster_new()),
    m_session(cass_session_new())
{
    cass_cluster_set_contact_points(m_cluster, host);
}

void Connection::initAsync(nx::utils::MoveOnlyFunc<void(CassError)> initCb)
{
    CassFuture* future = cass_session_connect(m_session, m_cluster);
    cass_future_set_callback(
        future,
        &onDone,
        makeErrorCbConnectionContext(this, std::move(initCb)));
    cass_future_free(future);
}

CassError Connection::initSync()
{
    CassFuture* future = cass_session_connect(m_session, m_cluster);
    auto result = cass_future_error_code(future);
    cass_future_free(future);

    return result;
}

void Connection::prepareQueryAsync(
    const char* queryString,
    nx::utils::MoveOnlyFunc<void(CassError, Query query)> prepareCb)
{
    CassFuture* future = cass_session_prepare(m_session, queryString);
    cass_future_set_callback(
        future,
        &onPrepare,
        makeQueryCbConnectionContext(this, std::move(prepareCb)));
    cass_future_free(future);
}

void Connection::executeSelectAsync(
    Query query,
    nx::utils::MoveOnlyFunc<void(CassError, const QueryResult&)> selectCb)
{
    CassFuture* future = cass_session_execute(m_session, query.m_statement);
    cass_future_set_callback(
        future,
        &onSelect,
        makeSelectCbConnectionContext(this, std::move(query), std::move(selectCb)));
    cass_future_free(future);
}

void Connection::executeUpdateAsync(
    Query query,
    nx::utils::MoveOnlyFunc<void(CassError)> executeCb)
{
    CassFuture* future = cass_session_connect(m_session, m_cluster);
    cass_future_set_callback(
        future,
        &onDone,
        makeErrorCbConnectionContext(this, std::move(initCb)));
    cass_future_free(future);
}

//void Connection::queryAsync(
//    const Query& /*query*/,
//    nx::utils::MoveOnlyFunc<void(CassError, const QueryResult&)> /*queryCb*/)
//{

//}

//boost::optional<QueryResult> Connection::querySync(const Query& /*query*/)
//{
//    return boost::none;
//}

Connection::~Connection()
{
    cass_session_free(m_session);
    cass_cluster_free(m_cluster);
}

} // namespace cassandra
} // namespace nx
