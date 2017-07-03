#include <string.h>
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
    ErrorCbConnectionContext* ctx = (ErrorCbConnectionContext*)data;
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
    QueryCbConnectionContext* ctx = (QueryCbConnectionContext*)data;
    ctx->cb(result, Query(future));
    ctx->free();
}


struct SelectCbConnectionContext
{
    nx::cassandra::Connection* ctx;
    Query query;
    MoveOnlyFunc<void(CassError, const QueryResult&)> cb;

    SelectCbConnectionContext(
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
    SelectCbConnectionContext* ctx = (SelectCbConnectionContext*)data;
    ctx->cb(result, QueryResult(future));
    ctx->free();
}


struct UpdateCbConnectionContext
{
    nx::cassandra::Connection* ctx;
    Query query;
    MoveOnlyFunc<void(CassError)> cb;

    UpdateCbConnectionContext(
        nx::cassandra::Connection* ctx,
        Query query,
        MoveOnlyFunc<void(CassError)> cb)
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

void* makeUpdateCbConnectionContext(
    Connection* connection,
    Query query,
    MoveOnlyFunc<void(CassError)> cb)
{
    return new UpdateCbConnectionContext(connection, std::move(query), std::move(cb));
}

static void onUpdate(CassFuture* future, void* data)
{
    CassError result = cass_future_error_code(future);
    UpdateCbConnectionContext* ctx = (UpdateCbConnectionContext*)data;
    ctx->cb(result);
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

Query::~Query()
{
    if (m_statement)
        cass_statement_free(m_statement);
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

#define NX_CASS_BIND_BASIC_VALUE(type) \
    if (!m_statement) \
        return false; \
    \
    auto result = cass_statement_bind_##type##_by_name_n( \
        m_statement, \
        key.data(), key.size(), \
        (cass_##type##_t)value); \
    \
    return result == CASS_OK;

bool Query::bind(const std::string& key, bool value)
{
    NX_CASS_BIND_BASIC_VALUE(bool);
}

bool Query::bind(const std::string& key, double value)
{
    NX_CASS_BIND_BASIC_VALUE(double);
}

bool Query::bind(const std::string& key, float value)
{
    NX_CASS_BIND_BASIC_VALUE(float);
}

bool Query::bind(const std::string& key, int32_t value)
{
    NX_CASS_BIND_BASIC_VALUE(int32);
}

bool Query::bind(const std::string& key, int64_t value)
{
    NX_CASS_BIND_BASIC_VALUE(int64);
}

#undef NX_CASS_BIND_BASIC_VALUE

/** ---------------------------------- QueryResult ----------------------------------------------*/

QueryResult::QueryResult(CassFuture* future)
{
    if (cass_future_error_code(future) != CASS_OK)
        return;

    m_result = cass_future_get_result(future);
    if (!m_result)
        return;

    m_iterator = cass_iterator_from_result(m_result);
}

QueryResult::~QueryResult()
{
    if (m_result)
        cass_result_free(m_result);

    if (m_iterator)
        cass_iterator_free(m_iterator);
}

bool QueryResult::next()
{
    if (!m_iterator)
        return false;

    return cass_iterator_next(m_iterator);
}

bool QueryResult::get(const std::string& key, std::string* value) const
{
    if (!m_iterator)
        return false;

    const CassRow* row = cass_iterator_get_row(m_iterator);
    if (!row)
        return false;

    const char* val;
    size_t valSize;
    auto result = cass_value_get_string(
                cass_row_get_column_by_name_n(row, key.data(), key.size()),
                &val,
                &valSize);

    if (result == CASS_OK)
    {
        value->resize(valSize);
        memcpy((void*)value->data(), val, valSize);
    }

    return result == CASS_OK;
}

#define NX_CASS_GET_BASIC_VALUE(type) \
    if (!m_iterator) \
        return false; \
    \
    const CassRow* row = cass_iterator_get_row(m_iterator); \
    if (!row) \
        return false; \
    \
    cass_##type##_t val; \
    auto result = cass_value_get_##type( \
            cass_row_get_column_by_name_n(row, key.data(), key.size()), \
            &val); \
    \
    if (result == CASS_OK) \
        *value = val; \
    \
    return result == CASS_OK;


bool QueryResult::get(const std::string& key, bool* value) const
{
    NX_CASS_GET_BASIC_VALUE(bool);
}

bool QueryResult::get(const std::string& key, double* value) const
{
    NX_CASS_GET_BASIC_VALUE(double);
}

bool QueryResult::get(const std::string& key, float* value) const
{
    NX_CASS_GET_BASIC_VALUE(float);
}

bool QueryResult::get(const std::string& key, int32_t* value) const
{
    NX_CASS_GET_BASIC_VALUE(int32);
}

bool QueryResult::get(const std::string& key, int64_t* value) const
{
    NX_CASS_GET_BASIC_VALUE(int64);
}

#undef NX_CASS_GET_BASIC_VALUE

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
    nx::utils::MoveOnlyFunc<void(CassError)> updateCb)
{
    CassFuture* future = cass_session_connect(m_session, m_cluster);
    cass_future_set_callback(
        future,
        &onUpdate,
        makeUpdateCbConnectionContext(this, std::move(query), std::move(updateCb)));
    cass_future_free(future);
}

Connection::~Connection()
{
    cass_session_free(m_session);
    cass_cluster_free(m_cluster);
}

} // namespace cassandra
} // namespace nx
