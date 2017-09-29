#include <string.h>
#include "async_cassandra_connection.h"

using namespace nx::utils;

namespace {
using namespace nx::cassandra;

struct ErrorCbContext
{
    nx::cassandra::AsyncConnection* ctx;
    MoveOnlyFunc<void(CassError)> cb;

    ErrorCbContext(nx::cassandra::AsyncConnection* ctx, MoveOnlyFunc<void(CassError)> cb):
        ctx(ctx),
        cb(std::move(cb))
    {}

    void free()
    {
        delete this;
    }
};

void* makeErrorCbContext(AsyncConnection* AsyncConnection, MoveOnlyFunc<void(CassError)> cb)
{
    return new ErrorCbContext(AsyncConnection, std::move(cb));
}

struct QueryCbContext
{
    nx::cassandra::AsyncConnection* ctx;
    MoveOnlyFunc<void(CassError, Query query)> cb;

    QueryCbContext(
        nx::cassandra::AsyncConnection* ctx,
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

void* makeQueryCbContext(
    AsyncConnection* AsyncConnection,
    MoveOnlyFunc<void(CassError, Query query)> cb)
{
    return new QueryCbContext(AsyncConnection, std::move(cb));
}


struct SelectCbContext
{
    nx::cassandra::AsyncConnection* ctx;
    Query query;
    MoveOnlyFunc<void(CassError, QueryResult)> cb;

    SelectCbContext(
        nx::cassandra::AsyncConnection* ctx,
        Query query,
        MoveOnlyFunc<void(CassError, QueryResult)> cb)
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

void* makeSelectCbContext(
    AsyncConnection* AsyncConnection,
    Query query,
    MoveOnlyFunc<void(CassError, QueryResult)> cb)
{
    return new SelectCbContext(AsyncConnection, std::move(query), std::move(cb));
}


struct UpdateCbContext
{
    nx::cassandra::AsyncConnection* ctx;
    Query query;
    MoveOnlyFunc<void(CassError)> cb;

    UpdateCbContext(
        nx::cassandra::AsyncConnection* ctx,
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

void* makeUpdateCbContext(
    AsyncConnection* AsyncConnection,
    Query query,
    MoveOnlyFunc<void(CassError)> cb)
{
    return new UpdateCbContext(AsyncConnection, std::move(query), std::move(cb));
}

} // namespace

namespace nx {
namespace cassandra {

/** ---------------------------------- Query ----------------------------------------------------*/

Query::Query(Query&& other):
    m_prepared(other.m_prepared),
    m_statement(other.m_statement)
{
    other.m_prepared = nullptr;
    other.m_statement = nullptr;
}

Query& Query::operator=(Query&& other)
{
    m_prepared = other.m_prepared;
    m_statement = other.m_statement;
    other.m_prepared = nullptr;
    other.m_statement = nullptr;

    return *this;
}

Query::Query(CassFuture* future)
{
    if (cass_future_error_code(future) != CASS_OK)
        return;

    m_prepared = cass_future_get_prepared(future);
    if (!m_prepared)
        return;

    m_statement = cass_prepared_bind(m_prepared);
}

Query::Query(CassStatement* statement):
    m_statement(statement)
{}

Query::~Query()
{
    if (m_statement)
        cass_statement_free(m_statement);

    if (m_prepared)
        cass_prepared_free(m_prepared);
}

bool Query::bind(const std::string& key, const char* value)
{
    if (!m_statement)
        return false;

    auto result = cass_statement_bind_string_by_name(m_statement, key.data(), value);

    return result == CASS_OK;
}

bool Query::bind(const std::string& key, const std::string& value)
{
    return bind(key, value.data());
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

bool Query::bind(const std::string& key, const InetAddr& value)
{
    if (!m_statement)
        return false;

    CassInet inetValue;
    cass_inet_from_string(value.addrString.c_str(), &inetValue);

    auto result = cass_statement_bind_inet_by_name_n(
        m_statement,
        key.data(), key.size(),
        inetValue);

    return result == CASS_OK;
}

bool Query::bind(const std::string& key, const Uuid& value)
{
    if (!m_statement)
        return false;

    CassUuid uuidValue;
    cass_uuid_from_string(value.uuidString.c_str(), &uuidValue);

    auto result = cass_statement_bind_uuid_by_name_n(
        m_statement,
        key.data(), key.size(),
        uuidValue);

    return result == CASS_OK;
}

bool Query::bindNull(const std::string& key)
{
    if (!m_statement)
        return false;

    auto result = cass_statement_bind_null_by_name_n(m_statement, key.data(), key.size());

    return result == CASS_OK;
}

#undef NX_CASS_BIND_BASIC_VALUE

/** ---------------------------------- QueryResult ----------------------------------------------*/

QueryResult::QueryResult(QueryResult&& other):
    m_result(other.m_result),
    m_iterator(other.m_iterator)
{
    other.m_result = nullptr;
    other.m_iterator = nullptr;
}

QueryResult& QueryResult::operator=(QueryResult&& other)
{
    m_result = other.m_result;
    m_iterator = other.m_iterator;
    other.m_result = nullptr;
    other.m_iterator = nullptr;

    return *this;
}

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

bool QueryResult::get(const std::string& key, boost::optional<std::string>* value) const
{
    return getStringImpl(
        [&key](const CassRow* row)
        {
            return cass_row_get_column_by_name_n(row, key.data(), key.size());
        },
        value);
}

#define NX_CASS_GET_BASIC_VALUE(type, getCassValueFactory) \
    if (!m_iterator) \
        return false; \
    \
    const CassRow* row = cass_iterator_get_row(m_iterator); \
    if (!row) \
        return false; \
    \
    const CassValue* cassValue = getCassValueFactory(row); \
    if (cass_value_is_null(cassValue)) \
    { \
        *value = boost::none; \
        return true; \
    } \
    \
    cass_##type##_t val; \
    auto result = cass_value_get_##type(getCassValueFactory(row), &val); \
    \
    if (result == CASS_OK) \
        *value = (type)val; \
    \
    return result == CASS_OK;


#define NX_CASS_GET_BASIC_VALUE_BY_INDEX(type) \
    NX_CASS_GET_BASIC_VALUE( \
        type, \
        [index](const CassRow* row) { return cass_row_get_column(row, index); })


#define NX_CASS_GET_BASIC_VALUE_BY_NAME(type) \
    NX_CASS_GET_BASIC_VALUE( \
        type, \
        [&key](const CassRow* row) \
        { \
            return cass_row_get_column_by_name_n(row, key.data(), key.size()); \
        })


bool QueryResult::get(const std::string& key, boost::optional<bool>* value) const
{
    NX_CASS_GET_BASIC_VALUE_BY_NAME(bool);
}

bool QueryResult::get(const std::string& key, boost::optional<double>* value) const
{
    NX_CASS_GET_BASIC_VALUE_BY_NAME(double);
}

bool QueryResult::get(const std::string& key, boost::optional<float>* value) const
{
    NX_CASS_GET_BASIC_VALUE_BY_NAME(float);
}

bool QueryResult::get(const std::string& key, boost::optional<int32_t>* value) const
{
    using int32 = int32_t;
    NX_CASS_GET_BASIC_VALUE_BY_NAME(int32);
}

bool QueryResult::get(const std::string& key, boost::optional<int64_t>* value) const
{
    using int64 = int64_t;
    NX_CASS_GET_BASIC_VALUE_BY_NAME(int64);
}

bool QueryResult::get(int index, boost::optional<std::string>* value) const
{
    return getStringImpl(
        [index](const CassRow* row)
        {
            return cass_row_get_column(row, index);
        },
        value);
}

bool QueryResult::get(int index, boost::optional<bool>* value) const
{
    NX_CASS_GET_BASIC_VALUE_BY_INDEX(bool);
}

bool QueryResult::get(int index, boost::optional<double>* value) const
{
    NX_CASS_GET_BASIC_VALUE_BY_INDEX(double);
}

bool QueryResult::get(int index, boost::optional<float>* value) const
{
    NX_CASS_GET_BASIC_VALUE_BY_INDEX(float);
}

bool QueryResult::get(int index, boost::optional<int32_t>* value) const
{
    using int32 = int32_t;
    NX_CASS_GET_BASIC_VALUE_BY_INDEX(int32);
}

bool QueryResult::get(int index, boost::optional<int64_t>* value) const
{
    using int64 = int64_t;
    NX_CASS_GET_BASIC_VALUE_BY_INDEX(int64);
}

bool QueryResult::get(const std::string& key, boost::optional<InetAddr>* value) const
{
    return getInetImpl(
        [&key](const CassRow* row)
        {
            return cass_row_get_column_by_name_n(row, key.data(), key.size());
        },
        value);
}

bool QueryResult::get(int index, boost::optional<InetAddr>* value) const
{
    return getInetImpl(
        [index](const CassRow* row)
        {
            return cass_row_get_column(row, index);
        },
        value);
}

bool QueryResult::get(const std::string& key, boost::optional<Uuid>* value) const
{
    return getUuidImpl(
        [&key](const CassRow* row)
        {
            return cass_row_get_column_by_name_n(row, key.data(), key.size());
        },
        value);
}

bool QueryResult::get(int index, boost::optional<Uuid>* value) const
{
    return getUuidImpl(
        [index](const CassRow* row)
        {
            return cass_row_get_column(row, index);
        },
        value);
}


#undef NX_CASS_GET_BASIC_VALUE_BY_NAME
#undef NX_CASS_GET_BASIC_VALUE_BY_INDEX
#undef NX_CASS_GET_BASIC_VALUE

//-------------------------------------------------------------------------------------------------
// AbstractAsyncConnection

cf::future<CassError> AbstractAsyncConnection::init()
{
    cf::promise<CassError> p;
    cf::future<CassError> f = p.get_future();

    init(
        [p = std::move(p)](CassError result) mutable
        {
            p.set_value(result);
        });

    return f;
}

cf::future<std::pair<CassError, Query>> AbstractAsyncConnection::prepareQuery(const char* queryString)
{
    cf::promise<std::pair<CassError, Query>> p;
    auto f = p.get_future();

    prepareQuery(
        queryString,
        [p = std::move(p)](CassError result, Query query) mutable
        {
            p.set_value(std::make_pair(result, std::move(query)));
        });

    return f;
}

cf::future<std::pair<CassError, QueryResult>> AbstractAsyncConnection::executeSelect(Query query)
{
    cf::promise<std::pair<CassError, QueryResult>> p;
    auto f = p.get_future();

    executeSelect(
        std::move(query),
        [p = std::move(p)](CassError result, QueryResult queryResult) mutable
        {
            p.set_value(std::make_pair(result, std::move(queryResult)));
        });

    return f;
}

cf::future<std::pair<CassError, QueryResult>> AbstractAsyncConnection::executeSelect(
        const char* queryString)
{
    cf::promise<std::pair<CassError, QueryResult>> p;
    auto f = p.get_future();

    executeSelect(
        queryString,
        [p = std::move(p)](CassError result, QueryResult queryResult) mutable
        {
            p.set_value(std::make_pair(result, std::move(queryResult)));
        });

    return f;
}

cf::future<CassError> AbstractAsyncConnection::executeUpdate(Query query)
{
    cf::promise<CassError> p;
    cf::future<CassError> f = p.get_future();

    executeUpdate(
        std::move(query),
        [p = std::move(p)](CassError result) mutable
        {
            p.set_value(result);
        });

    return f;
}

cf::future<CassError> AbstractAsyncConnection::executeUpdate(const char* queryString)
{
    cf::promise<CassError> p;
    cf::future<CassError> f = p.get_future();

    executeUpdate(
        queryString,
        [p = std::move(p)](CassError result) mutable
        {
            p.set_value(result);
        });

    return f;
}

//-------------------------------------------------------------------------------------------------
// AsyncConnection

AsyncConnection::AsyncConnection(const std::string& hostName):
    m_cluster(cass_cluster_new()),
    m_session(cass_session_new())
{
    cass_cluster_set_contact_points(m_cluster, hostName.c_str());
}

void AsyncConnection::init(nx::utils::MoveOnlyFunc<void(CassError)> initCb)
{
    incPending();
    CassFuture* future = cass_session_connect(m_session, m_cluster);
    cass_future_set_callback(
        future,
        &onDone,
        makeErrorCbContext(this, std::move(initCb)));
    cass_future_free(future);
}

void AsyncConnection::prepareQuery(
    const char* queryString,
    nx::utils::MoveOnlyFunc<void(CassError, Query query)> prepareCb)
{
    incPending();
    CassFuture* future = cass_session_prepare(m_session, queryString);
    cass_future_set_callback(
        future,
        &onPrepare,
        makeQueryCbContext(this, std::move(prepareCb)));
    cass_future_free(future);
}

void AsyncConnection::executeSelect(
    Query query,
    nx::utils::MoveOnlyFunc<void(CassError, QueryResult)> selectCb)
{
    incPending();
    CassFuture* future = cass_session_execute(m_session, query.m_statement);
    cass_future_set_callback(
        future,
        &onSelect,
        makeSelectCbContext(this, std::move(query), std::move(selectCb)));
    cass_future_free(future);
}

void AsyncConnection::executeSelect(
    const char* queryString,
    nx::utils::MoveOnlyFunc<void(CassError, QueryResult result)> selectCb)
{
    incPending();
    CassStatement* statement = cass_statement_new(queryString, 0);
    CassFuture* future = cass_session_execute(m_session, statement);
    cass_future_set_callback(
        future,
        &onSelect,
        makeSelectCbContext(this, Query(statement), std::move(selectCb)));
    cass_future_free(future);
}

void AsyncConnection::executeUpdate(
    Query query,
    nx::utils::MoveOnlyFunc<void(CassError)> updateCb)
{
    incPending();
    CassFuture* future = cass_session_execute(m_session, query.m_statement);
    cass_future_set_callback(
        future,
        &onUpdate,
        makeUpdateCbContext(this, std::move(query), std::move(updateCb)));
    cass_future_free(future);
}

void AsyncConnection::executeUpdate(
    const char* queryString,
    nx::utils::MoveOnlyFunc<void(CassError)> updateCb)
{
    incPending();
    CassStatement* statement = cass_statement_new(queryString, 0);
    CassFuture* future = cass_session_execute(m_session, statement);
    cass_future_set_callback(
        future,
        &onUpdate,
        makeUpdateCbContext(this, Query(statement), std::move(updateCb)));
    cass_future_free(future);
}

AsyncConnection::~AsyncConnection()
{
    cass_session_free(m_session);
    cass_cluster_free(m_cluster);
}

void AsyncConnection::wait()
{
    QnMutexLocker lock(&m_mutex);
    while (m_pendingCount != 0)
        m_cond.wait(lock.mutex());
}

void AsyncConnection::incPending()
{
    QnMutexLocker lock(&m_mutex);
    ++m_pendingCount;
}

void AsyncConnection::decPending()
{
    QnMutexLocker lock(&m_mutex);
    if (--m_pendingCount == 0)
        m_cond.wakeAll();
}

void AsyncConnection::onDone(CassFuture* future, void* data)
{
    CassError result = cass_future_error_code(future);
    ErrorCbContext* ctx = (ErrorCbContext*)data;
    ctx->cb(result);
    ctx->ctx->decPending();
    ctx->free();
}

void AsyncConnection::onPrepare(CassFuture* future, void* data)
{
    CassError result = cass_future_error_code(future);
    QueryCbContext* ctx = (QueryCbContext*)data;
    ctx->cb(result, Query(future));
    ctx->ctx->decPending();
    ctx->free();
}

void AsyncConnection::onSelect(CassFuture* future, void* data)
{
    CassError result = cass_future_error_code(future);
    SelectCbContext* ctx = (SelectCbContext*)data;
    ctx->cb(result, QueryResult(future));
    ctx->ctx->decPending();
    ctx->free();
}

void AsyncConnection::onUpdate(CassFuture* future, void* data)
{
    CassError result = cass_future_error_code(future);
    UpdateCbContext* ctx = (UpdateCbContext*)data;
    ctx->cb(result);
    ctx->ctx->decPending();
    ctx->free();
}

//-------------------------------------------------------------------------------------------------

AsyncConnectionFactory::AsyncConnectionFactory():
    base_type(std::bind(&AsyncConnectionFactory::defaultFactoryFunction, this,
        std::placeholders::_1))
{
}

AsyncConnectionFactory& AsyncConnectionFactory::instance()
{
    static AsyncConnectionFactory staticInstance;
    return staticInstance;
}

std::unique_ptr<AbstractAsyncConnection>
    AsyncConnectionFactory::defaultFactoryFunction(const std::string& dbHostName)
{
    return std::make_unique<AsyncConnection>(dbHostName);
}

} // namespace cassandra
} // namespace nx
